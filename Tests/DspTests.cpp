// Offline checks of the audio code: feeds test signals through each effect and measures the result.
// Build with -DGUITARDRIVE_BUILD_TESTS=ON, target GuitarDriveTests. Exit code is the number of failures.
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "ChannelStrip.h"
#include "Effects.h"
#include "Logo.h"
#include "Metronome.h"
#include "PlateReverb.h"
#include "Presets.h"
#include "SpringReverb.h"
#include "Tuner.h"

namespace
{
    constexpr double sr = 48000.0;
    int failures = 0;

    void check (const char* name, bool ok, const char* detail)
    {
        std::printf ("%s  %-44s %s\n", ok ? "PASS" : "FAIL", name, detail);
        if (! ok)
            ++failures;
    }

    std::mt19937 rng (12345);
    float noise (float amp) { return amp * (std::uniform_real_distribution<float> (-1.0f, 1.0f) (rng)); }

    std::vector<float> sine (double f, float amp, int n)
    {
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = amp * (float) std::sin (6.283185307179586 * f * i / sr);
        return v;
    }

    double rms (const std::vector<float>& v, int a, int b)
    {
        double s = 0.0;
        for (int i = a; i < b; ++i)
            s += (double) v[(size_t) i] * v[(size_t) i];
        return std::sqrt (s / std::max (1, b - a));
    }

    // amplitude of one frequency (single-bin DFT)
    double bin (const std::vector<float>& v, int a, int b, double f)
    {
        double re = 0.0, im = 0.0;
        for (int i = a; i < b; ++i)
        {
            const double w = 6.283185307179586 * f * i / sr;
            re += v[(size_t) i] * std::cos (w);
            im -= v[(size_t) i] * std::sin (w);
        }
        return 2.0 * std::sqrt (re * re + im * im) / (b - a);
    }

    double db (double ratio) { return 20.0 * std::log10 (ratio + 1.0e-12); }

    bool allFinite (const std::vector<float>& v)
    {
        for (float x : v)
            if (! std::isfinite (x))
                return false;
        return true;
    }

    char buf[256];

    template <typename F>
    void inBlocks (std::vector<float>& x, int block, F&& f)
    {
        for (size_t start = 0; start < x.size(); start += (size_t) block)
            f (x.data() + start, (int) std::min ((size_t) block, x.size() - start));
    }

    // Frequency of the strongest partial near `around`, from a fine single-bin scan of the last second.
    // (More precise than the tuner's detector, whose window is short for low notes that wobble in level.)
    double peakFrequency (const std::vector<float>& x, double around)
    {
        double bestF = 0.0, best = 0.0;
        for (double f = around * 0.85; f < around * 1.15; f += 0.05)
        {
            const double m = bin (x, (int) x.size() / 3, (int) x.size(), f);
            if (m > best)
            {
                best = m;
                bestF = f;
            }
        }
        return bestF;
    }

    // a synthetic picked note: a short noise click on top of decaying harmonics
    std::vector<float> pluck (double f0, int n)
    {
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            double s = 0.6 * noise (1.0f) * std::exp (-t / 0.0008);
            for (int h = 1; h <= 8; ++h)
                s += 0.3 / h * std::sin (6.283185307179586 * f0 * h * t) * std::exp (-t / 0.6);
            v[(size_t) i] = (float) s;
        }
        return v;
    }

    void testGate()
    {
        fx::NoiseGate gate;
        gate.prepare (sr);
        std::vector<float> hiss ((size_t) sr), in;
        for (auto& v : hiss)
            v = noise (0.003f);   // about -50 dBFS
        in = hiss;
        gate.process (hiss.data(), (int) hiss.size(), 7.0f);
        std::snprintf (buf, sizeof buf, "hiss attenuated by %.1f dB", -db (rms (hiss, 24000, 48000) / rms (in, 24000, 48000)));
        check ("gate closes on hiss", db (rms (hiss, 24000, 48000) / rms (in, 24000, 48000)) < -20.0, buf);

        gate.reset();
        auto tone = sine (196.0, 0.3f, (int) sr);
        auto toneIn = tone;
        gate.process (tone.data(), (int) tone.size(), 7.0f);
        std::snprintf (buf, sizeof buf, "tone gain %.2f dB", db (rms (tone, 24000, 48000) / rms (toneIn, 24000, 48000)));
        check ("gate passes a played note", std::abs (db (rms (tone, 24000, 48000) / rms (toneIn, 24000, 48000))) < 1.0, buf);

        auto off = sine (196.0, 0.3f, 4800);
        auto offIn = off;
        fx::NoiseGate g2;
        g2.prepare (sr);
        g2.process (off.data(), (int) off.size(), 0.0f);
        check ("gate knob at 0 is bypass", std::abs (db (rms (off, 2400, 4800) / rms (offIn, 2400, 4800))) < 0.1, "unity");
    }

    void testTransposer()
    {
        const struct { float semis; double expect; } cases[] = { { 12.0f, 440.0 }, { -12.0f, 110.0 }, { 7.0f, 329.63 }, { -5.0f, 164.81 }, { 3.0f, 261.63 } };
        for (const auto& c : cases)
        {
            fx::PitchShifter ps;
            ps.prepare (sr);
            auto x = sine (220.0, 0.3f, (int) (sr * 1.5));
            inBlocks (x, 256, [&] (float* d, int n) { ps.process (d, n, c.semis); });
            const double f = peakFrequency (x, c.expect);
            const double cents = 1200.0 * std::log2 (f / c.expect);
            std::snprintf (buf, sizeof buf, "%+.0f st: %.2f Hz (want %.2f), %.1f cents off", c.semis, f, c.expect, cents);
            check ("transposer pitch", f > 0.0 && std::abs (cents) < 5.0, buf);
        }

        fx::PitchShifter ps;
        ps.prepare (sr);
        auto x = sine (220.0, 0.3f, 9600);
        const auto in = x;
        inBlocks (x, 256, [&] (float* d, int n) { ps.process (d, n, 0.0f); });
        double diff = 0.0;
        for (size_t i = 0; i < x.size(); ++i)
            diff = std::max (diff, (double) std::abs (x[i] - in[i]));
        check ("transposer at 0 is bypass", diff < 1.0e-6, "identical");
    }

    void testCompressor()
    {
        auto run = [] (float amp)
        {
            fx::Compressor c;
            c.prepare (sr);
            auto x = sine (440.0, amp, (int) sr);
            const auto in = x;
            inBlocks (x, 256, [&] (float* d, int n) { c.process (d, n, 7.0f, 3.0f, 5.0f); });
            return db (rms (x, 24000, 48000) / rms (in, 24000, 48000));
        };
        const double loud = run (0.5f), quiet = run (0.02f);
        std::snprintf (buf, sizeof buf, "gain %.1f dB on loud, %.1f dB on quiet", loud, quiet);
        check ("compressor evens out levels", quiet - loud > 3.0, buf);
    }

    void testWah()
    {
        auto energy = [] (float pos, double f)
        {
            fx::Wah w;
            w.prepare (sr);
            std::vector<float> x ((size_t) (sr * 1.0));
            for (auto& v : x)
                v = noise (0.3f);
            inBlocks (x, 256, [&] (float* d, int n) { w.process (d, n, false, pos, 5.0f, 5.0f); });
            return bin (x, 12000, 48000, f);
        };
        const double heel400 = energy (0.0f, 420.0), heel2k = energy (0.0f, 2000.0);
        const double toe400 = energy (10.0f, 420.0), toe2k = energy (10.0f, 2000.0);
        std::snprintf (buf, sizeof buf, "heel: 420Hz %.4f vs 2k %.4f | toe: 420Hz %.4f vs 2k %.4f", heel400, heel2k, toe400, toe2k);
        check ("wah sweeps from low to high", heel400 > heel2k * 1.5 && toe2k > toe400 * 1.5, buf);

        fx::Wah w;
        w.prepare (sr);
        auto x = pluck (196.0, (int) sr);
        inBlocks (x, 256, [&] (float* d, int n) { w.process (d, n, true, 3.0f, 6.0f, 6.0f); });
        check ("auto wah stays finite", allFinite (x), "ok");
    }

    void testDrives()
    {
        auto harmonic = [] (bool screamer, float drive)
        {
            auto x = sine (330.0, 0.2f, (int) sr);
            fx::TubeScreamer ts;
            fx::Overdrive od;
            ts.prepare (sr, 256);
            od.prepare (sr, 256);
            inBlocks (x, 256, [&] (float* d, int n)
            {
                if (screamer) ts.process (d, n, drive, 5.0f, 5.0f);
                else od.process (d, n, drive, 5.0f, 5.0f);
            });
            const double fundamental = bin (x, 24000, 48000, 330.0);
            return std::make_pair (db (bin (x, 24000, 48000, 990.0) / fundamental), allFinite (x));
        };
        for (bool screamer : { true, false })
        {
            const auto low = harmonic (screamer, 0.5f), high = harmonic (screamer, 8.0f);
            std::snprintf (buf, sizeof buf, "3rd harmonic %.1f dB at drive 0.5, %.1f dB at drive 8", low.first, high.first);
            check (screamer ? "tube screamer distorts with drive" : "overdrive distorts with drive",
                   low.second && high.second && high.first > low.first + 10.0, buf);
        }
    }

    void testEq()
    {
        auto gainAt = [] (double f, int band, float gainDb)
        {
            fx::GraphicEq eq;
            eq.prepare (sr, 256);
            std::array<float, Params::numEqBands> g {};
            g[(size_t) band] = gainDb;
            auto l = sine (f, 0.1f, (int) sr), r = l;
            const auto in = l;
            for (size_t s = 0; s < l.size(); s += 256)
                eq.process (l.data() + s, r.data() + s, (int) std::min<size_t> (256, l.size() - s), g);
            return db (rms (l, 24000, 48000) / rms (in, 24000, 48000));
        };
        const double boosted = gainAt (1000.0, 4, 12.0f), away = gainAt (100.0, 4, 12.0f), flat = gainAt (1000.0, 4, 0.0f);
        std::snprintf (buf, sizeof buf, "1 kHz boosted %+.1f dB, 100 Hz %+.1f dB, flat %+.2f dB", boosted, away, flat);
        check ("equalizer boosts only its band", std::abs (boosted - 12.0) < 1.5 && std::abs (away) < 1.0 && std::abs (flat) < 0.2, buf);
    }

    void testPostFx()
    {
        fx::DelayFx delay;
        delay.prepare (sr);
        std::vector<float> l ((size_t) sr, 0.0f), r (l);
        l[0] = r[0] = 1.0f;
        for (size_t s = 0; s < l.size(); s += 256)
            delay.process (l.data() + s, r.data() + s, 256 > (int) (l.size() - s) ? (int) (l.size() - s) : 256, true, 380.0f, 3.0f, 10.0f, 10.0f);
        size_t peak = 1000;
        for (size_t i = 1000; i < l.size(); ++i)
            if (std::abs (l[i]) > std::abs (l[peak]))
                peak = i;
        std::snprintf (buf, sizeof buf, "echo at %.1f ms (want 380)", peak * 1000.0 / sr);
        check ("delay echo lands on the set time", std::abs ((double) peak * 1000.0 / sr - 380.0) < 3.0, buf);

        fx::Tremolo trem;
        trem.prepare (sr);
        auto a = sine (220.0, 0.3f, (int) sr), b = a;
        trem.process (a.data(), b.data(), (int) a.size(), true, 6.0f, 10.0f);
        double lo = 1.0e9, hi = 0.0;
        for (int s = 24000; s + 960 < 48000; s += 960)
        {
            const double e = rms (a, s, s + 960);
            lo = std::min (lo, e);
            hi = std::max (hi, e);
        }
        std::snprintf (buf, sizeof buf, "level swings from %.3f to %.3f", lo, hi);
        check ("tremolo modulates the level", hi > 3.0 * lo, buf);

        fx::PostFx post;
        post.prepare (sr, 256);
        fx::PostParams pp;
        pp.phOn = pp.chOn = pp.trOn = pp.dlOn = pp.rvOn = true;
        pp.phRate = 3; pp.phDepth = 5; pp.phFb = 3; pp.phMix = 5;
        pp.chRate = 3; pp.chDepth = 5; pp.chMix = 5;
        pp.trRate = 5; pp.trDepth = 5;
        pp.dlTime = 380; pp.dlFb = 3; pp.dlTone = 5; pp.dlMix = 3;
        pp.rvSize = 5; pp.rvDamp = 5; pp.rvMix = 3;
        auto pl = pluck (196.0, (int) (sr * 2)), pr = pl;
        for (size_t s = 0; s < pl.size(); s += 256)
            post.process (pl.data() + s, pr.data() + s, (int) std::min<size_t> (256, pl.size() - s), pp);
        check ("all post effects on stay finite", allFinite (pl) && allFinite (pr) && rms (pl, 0, 48000) > 0.01, "ok");
    }

    void testMetronome()
    {
        Metronome m;
        m.prepare (sr);
        std::vector<float> l ((size_t) (sr * 4.1)), r (l.size());
        for (size_t s = 0; s < l.size(); s += 512)
            m.process (l.data() + s, r.data() + s, (int) std::min<size_t> (512, l.size() - s), true, 120.0f, 4, 1, 8.0f);

        std::vector<double> peaks;
        for (size_t i = 0; i < l.size();)
        {
            if (std::abs (l[i]) > 0.02f)
            {
                double peak = 0.0;
                for (size_t j = i; j < std::min (l.size(), i + 2400); ++j)
                    peak = std::max (peak, (double) std::abs (l[j]));
                peaks.push_back (peak);
                i += 12000;   // skip to the next pulse
            }
            else
                ++i;
        }
        std::snprintf (buf, sizeof buf, "%zu clicks in 4.1 s at 120 BPM", peaks.size());
        check ("metronome pulse count (4/4 at 120 BPM)", peaks.size() == 9 || peaks.size() == 8, buf);
        check ("metronome accents the first beat", peaks.size() >= 2 && peaks[0] > peaks[1] * 1.2, "first click is louder");
        check ("metronome 6/8 groups in threes", Metronome::accentFor (6, 8, 3) == 1 && Metronome::accentFor (6, 8, 0) == 2 && Metronome::accentFor (6, 8, 1) == 0, "accents on 1 and 4");
        check ("metronome 4/4 has one accent", Metronome::accentFor (4, 4, 0) == 2 && Metronome::accentFor (4, 4, 2) == 0, "beat 1 only");
        check ("metronome 7/8 groups 2+2+3", Metronome::accentFor (7, 8, 2) == 1 && Metronome::accentFor (7, 8, 4) == 1 && Metronome::accentFor (7, 8, 3) == 0, "accents on 1, 3 and 5");
        check ("typed note values snap to 2, 4, 8 or 16", Metronome::noteValue (Metronome::noteIndexFor (5)) == 4 && Metronome::noteValue (Metronome::noteIndexFor (7)) == 8 && Metronome::noteValue (Metronome::noteIndexFor (13)) == 16, "5 -> 4, 7 -> 8, 13 -> 16");
    }

    // click-tail brightness: share of the wet energy above 2 kHz, 30..400 ms after a pick click
    struct ReverbReport { double tailDb, hfShareDb; bool finite; };

    template <typename Reverb>
    ReverbReport clickTail (Reverb& rev)
    {
        double eTot = 0.0, eHf = 0.0, eIn = 0.0;
        bool finite = true;
        // second-order high-pass at 2 kHz
        const double w = 6.283185307179586 * 2000.0 / sr, c = std::cos (w), s = std::sin (w), al = s / 1.4142, a0 = 1.0 + al;
        const double b0 = (1 + c) / 2 / a0, b1 = -(1 + c) / a0, b2 = b0, a1 = -2 * c / a0, a2 = (1 - al) / a0;
        double z1 = 0.0, z2 = 0.0;

        for (int i = 0; i < (int) sr; ++i)
        {
            const double t = i / sr;
            const float x = (float) (0.6 * noise (1.0f) * std::exp (-t / 0.0008));
            eIn += (double) x * x;
            float l, rr;
            rev.process (x, l, rr);
            finite = finite && std::isfinite (l) && std::isfinite (rr);
            const double y = l, h = b0 * y + z1;
            z1 = b1 * y - a1 * h + z2;
            z2 = b2 * y - a2 * h;
            if (t >= 0.03 && t < 0.4)
            {
                eTot += y * y;
                eHf += h * h;
            }
        }
        return { 10.0 * std::log10 (eTot / eIn), 10.0 * std::log10 (eHf / eTot), finite };
    }

    template <typename Reverb>
    double decayDb (Reverb& rev)   // level 2.5-3 s after a short burst, relative to the first half second
    {
        std::vector<float> out ((size_t) (sr * 3.0));
        for (int i = 0; i < (int) out.size(); ++i)
        {
            float l, rr;
            rev.process (i < 2400 ? noise (0.3f) : 0.0f, l, rr);
            out[(size_t) i] = l;
        }
        return db (rms (out, (int) (sr * 2.5), (int) (sr * 3.0)) / rms (out, 0, (int) (sr * 0.5)));
    }

    // a reverb fed through the pick-attack ducker, as in the channel strip
    template <typename Reverb>
    struct DuckedReverb
    {
        fx::ReverbDuck duck;
        Reverb reverb;
        void prepare (double rate) { duck.prepare (rate); reverb.prepare (rate); }
        void reset() { duck.reset(); reverb.reset(); }
        void process (float in, float& l, float& r) { reverb.process (duck.process (in), l, r); }
    };

    void testReverbs()
    {
        PlateReverb plate;
        SpringReverb spring;
        DuckedReverb<PlateReverb> duckedPlate;
        DuckedReverb<SpringReverb> duckedSpring;
        plate.prepare (sr);
        spring.prepare (sr);
        duckedPlate.prepare (sr);
        duckedSpring.prepare (sr);

        const auto p = clickTail (plate);
        const auto s = clickTail (spring);
        const auto dp = clickTail (duckedPlate);
        const auto ds = clickTail (duckedSpring);

        // absolute high-frequency energy in the tail, relative to the click that went in
        const auto hf = [] (const ReverbReport& r) { return r.tailDb + r.hfShareDb; };
        std::snprintf (buf, sizeof buf, "HF tail energy (dB re click): spring %.1f, plate %.1f | with ducking: spring %.1f, plate %.1f",
                       hf (s), hf (p), hf (ds), hf (dp));
        check ("ducking removes the pick hiss from the reverb",
               s.finite && p.finite && ds.finite && dp.finite && hf (ds) < hf (s) - 8.0 && hf (dp) < hf (p) - 8.0, buf);

        // a held note must still reach the reverb
        {
            DuckedReverb<PlateReverb> r;
            r.prepare (sr);
            const auto note = sine (196.0, 0.3f, (int) sr);
            double wet = 0.0;
            for (size_t i = 0; i < note.size(); ++i)
            {
                float l, rr;
                r.process (note[i], l, rr);
                if (i > 24000)
                    wet += (double) l * l;
            }
            wet = std::sqrt (wet / 24000.0);
            std::snprintf (buf, sizeof buf, "plate output on a held note: rms %.3f (input 0.212)", wet);
            check ("a held note still feeds the reverb", wet > 0.03, buf);
        }

        plate.reset();
        spring.reset();

        plate.reset();
        spring.reset();
        const double pd = decayDb (plate), sd = decayDb (spring);
        std::snprintf (buf, sizeof buf, "plate %.1f dB, spring %.1f dB after 2.5 s", pd, sd);
        check ("reverbs decay and stay stable", pd < -35.0 && sd < -35.0, buf);

        // loud, long soak
        plate.reset();
        double peak = 0.0;
        for (int i = 0; i < (int) (sr * 20); ++i)
        {
            float l, rr;
            plate.process (noise (1.0f), l, rr);
            peak = std::max (peak, (double) std::max (std::abs (l), std::abs (rr)));
        }
        std::snprintf (buf, sizeof buf, "peak %.2f after 20 s of full-scale noise", peak);
        check ("plate reverb soak stays bounded", std::isfinite (peak) && peak < 3.0, buf);
    }

    // a tiny processor with a few parameters, to exercise the preset manager without the whole rig
    struct TestProcessor : juce::AudioProcessor
    {
        TestProcessor() = default;
        const juce::String getName() const override { return "test"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
        {
            std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
            p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gain", 1 }, "Gain", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));
            p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "r_gain", 1 }, "R Gain", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f));
            p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "ts_drive", 1 }, "Drive", juce::NormalisableRange<float> (0.0f, 10.0f, 0.01f), 2.5f));
            p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "ts_on", 1 }, "TS", false));
            p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "metro_on", 1 }, "Metro", false));
            return { p.begin(), p.end() };
        }

        juce::AudioProcessorValueTreeState apvts { *this, nullptr, "S", makeLayout() };

        void set (const char* id, float denormalised)
        {
            auto* p = apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (denormalised));
        }
        float get (const char* id) { auto* p = apvts.getParameter (id); return p->convertFrom0to1 (p->getValue()); }
    };

    void testPresets()
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("jgr-preset-test-" + juce::String (juce::Random::getSystemRandom().nextInt (1000000)));
        {
            TestProcessor proc;
            PresetManager pm (proc.apvts, proc, dir);

            const auto factory = pm.names();
            check ("factory presets are created on first run",
                   factory.contains ("Init") && factory.contains ("Mayer Clean") && factory.contains ("SRV Edge"), factory.joinIntoString (", ").toRawUTF8());

            pm.load ("SRV Edge");
            std::snprintf (buf, sizeof buf, "ts_on %.0f, ts_drive %.1f, gain right copy %.1f", proc.get ("ts_on"), proc.get ("ts_drive"), proc.get ("r_gain"));
            check ("loading a preset applies its values (both channels)", proc.get ("ts_on") > 0.5f && std::abs (proc.get ("ts_drive") - 3.0f) < 0.05f
                                                                              && std::abs (proc.get ("r_gain") - 5.5f) < 0.05f, buf);
            check ("a freshly loaded preset is not marked modified", pm.currentName() == "SRV Edge" && ! pm.isDirty(), "no asterisk");

            proc.set ("gain", 8.0f);
            check ("editing a loaded preset marks it modified", pm.isDirty(), "asterisk");

            check ("saving clears the modified mark", pm.save ("SRV Edge") && ! pm.isDirty(), "asterisk gone");

            proc.set ("metro_on", 1.0f);
            check ("the metronome is not part of a preset", ! pm.isDirty(), "toggling it changes nothing");

            proc.set ("gain", 2.0f);
            check ("saving under a new name creates a preset", pm.save ("My Tone") && pm.names().contains ("My Tone") && pm.currentName() == "My Tone", "My Tone");

            proc.set ("gain", 9.0f);
            pm.load ("My Tone");
            check ("a saved preset loads back exactly", std::abs (proc.get ("gain") - 2.0f) < 0.05f && ! pm.isDirty(), "gain 2.0");

            check ("deleting a preset removes it and the selection", pm.remove ("My Tone") && ! pm.names().contains ("My Tone") && pm.currentName().isEmpty(), "gone");

            pm.load ("Init");
            proc.set ("gain", 7.0f);
            check ("the name is kept in the plugin state", proc.apvts.state.getProperty ("preset").toString() == "Init", "Init");
            pm.restoreFromState();
            check ("modified state is recognised after the host restores it", pm.isDirty(), "asterisk survives a reload");
        }
        dir.deleteRecursively();
    }

    struct StripFixture
    {
        std::atomic<float> gate { 0 }, trans { 0 }, gain { 5 }, warmth { 3 }, bass { 5 }, mid { 5 }, treble { 5 }, reverb { 2.5f }, master { 8 }, revType { 0 };
        std::atomic<float> wahOn { 0 }, wahAuto { 0 }, wahPos { 5 }, wahSens { 5 }, wahQ { 5 };
        std::atomic<float> compOn { 0 }, compSustain { 3 }, compAttack { 4 }, compLevel { 5 };
        std::atomic<float> tsOn { 0 }, tsDrive { 2.5f }, tsTone { 5.5f }, tsLevel { 6.5f };
        std::atomic<float> odOn { 0 }, odDrive { 4 }, odTone { 5 }, odLevel { 5 };
        std::atomic<float> off { 0 }, one { 1 }, five { 5 }, delayTime { 380 };   // equalizer and post effects stay flat / off

        ChannelStrip::Params params()
        {
            ChannelStrip::Params p;
            p.eqOn = &one;
            p.eq.fill (&off);
            auto& q = p.post;
            q.phOn = q.chOn = q.trOn = q.dlOn = q.rvOn = &off;
            q.phRate = q.phDepth = q.phFb = q.phMix = &five;
            q.chRate = q.chDepth = q.chMix = &five;
            q.trRate = q.trDepth = &five;
            q.dlTime = &delayTime;
            q.dlFb = q.dlTone = q.dlMix = &five;
            q.rvSize = q.rvDamp = q.rvMix = &five;
            p.gate = &gate; p.trans = &trans; p.gain = &gain; p.warmth = &warmth; p.bass = &bass; p.mid = &mid;
            p.treble = &treble; p.reverb = &reverb; p.master = &master; p.revType = &revType;
            p.wahOn = &wahOn; p.wahAuto = &wahAuto; p.wahPos = &wahPos; p.wahSens = &wahSens; p.wahQ = &wahQ;
            p.compOn = &compOn; p.compSustain = &compSustain; p.compAttack = &compAttack; p.compLevel = &compLevel;
            p.tsOn = &tsOn; p.tsDrive = &tsDrive; p.tsTone = &tsTone; p.tsLevel = &tsLevel;
            p.odOn = &odOn; p.odDrive = &odDrive; p.odTone = &odTone; p.odLevel = &odLevel;
            return p;
        }
    };

    struct StripResult { bool finite; double leftRms, rightRms, peak; };

    StripResult runStrip (StripFixture& fx, int blockSize = 256)
    {
        ChannelStrip strip;
        strip.setParams (fx.params());
        strip.prepare (sr, blockSize);

        const auto in = pluck (196.0, (int) (sr * 2.0));
        std::vector<float> l (in.size()), r (in.size());
        for (size_t s = 0; s < in.size(); s += (size_t) blockSize)
        {
            const int n = (int) std::min<size_t> ((size_t) blockSize, in.size() - s);
            strip.process (in.data() + s, l.data() + s, r.data() + s, n);
        }

        double peak = 0.0;
        for (size_t i = 0; i < l.size(); ++i)
            peak = std::max (peak, (double) std::max (std::abs (l[i]), std::abs (r[i])));
        return { allFinite (l) && allFinite (r), rms (l, 0, 48000), rms (r, 0, 48000), peak };
    }

    void testStrip()
    {
        StripFixture plain;
        auto res = runStrip (plain);
        std::snprintf (buf, sizeof buf, "L rms %.4f, R rms %.4f, peak %.2f", res.leftRms, res.rightRms, res.peak);
        check ("strip: mono in gives sound in both channels", res.finite && res.leftRms > 0.005 && res.rightRms > 0.005, buf);

        StripFixture spring;
        spring.revType = 1.0f;
        res = runStrip (spring);
        check ("strip: spring reverb type works", res.finite && res.leftRms > 0.005 && res.rightRms > 0.005, "both channels live");

        StripFixture all;
        all.gate = 5.0f; all.trans = 3.0f; all.wahOn = 1.0f; all.compOn = 1.0f; all.tsOn = 1.0f; all.odOn = 1.0f;
        all.odDrive = 9.0f; all.warmth = 8.0f; all.reverb = 7.0f;
        res = runStrip (all, 100);   // odd block size on purpose
        std::snprintf (buf, sizeof buf, "peak %.2f, L rms %.4f", res.peak, res.leftRms);
        check ("strip: every pedal on, odd block size", res.finite && res.leftRms > 0.001 && res.peak < 8.0, buf);

        // the screamer should add edge but not be wildly louder than the clean sound at its default settings
        StripFixture ts;
        ts.tsOn = 1.0f;
        const auto withTs = runStrip (ts);
        std::snprintf (buf, sizeof buf, "clean %.4f, with screamer %.4f (%.1f dB)", res.leftRms, withTs.leftRms, db (withTs.leftRms / std::max (1.0e-9, runStrip (plain).leftRms)));
        check ("strip: screamer default is a gentle push", withTs.finite && db (withTs.leftRms / std::max (1.0e-9, runStrip (plain).leftRms)) < 9.0, buf);
    }
}

// ---------------------------------------------------------------- tone report (run with --tone)
// Where does the sound gain or lose energy? Octave-band levels of the output compared with the input.
namespace
{
    constexpr int numBands = 7;
    const double bandEdges[numBands + 1] = { 80, 160, 320, 640, 1280, 2560, 5120, 10240 };

    std::array<double, numBands> bandLevels (const std::vector<float>& x, int start)
    {
        constexpr int order = 16, n = 1 << order;
        std::vector<float> data ((size_t) n * 2, 0.0f);
        for (int i = 0; i < n && start + i < (int) x.size(); ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) (n - 1));   // Hann window
            data[(size_t) i] = x[(size_t) (start + i)] * w;
        }
        juce::dsp::FFT fft (order);
        fft.performFrequencyOnlyForwardTransform (data.data());

        std::array<double, numBands> power {};
        for (int k = 1; k < n / 2; ++k)
        {
            const double f = (double) k * sr / n;
            for (int b = 0; b < numBands; ++b)
                if (f >= bandEdges[b] && f < bandEdges[b + 1])
                    power[(size_t) b] += (double) data[(size_t) k] * data[(size_t) k];
        }
        for (auto& p : power)
            p = 10.0 * std::log10 (p + 1.0e-20);
        return power;
    }

    std::vector<float> renderLeft (StripFixture& fixture, const std::vector<float>& in, int block = 256)
    {
        ChannelStrip strip;
        strip.setParams (fixture.params());
        strip.prepare (sr, block);
        std::vector<float> l (in.size()), r (in.size());
        for (size_t s = 0; s < in.size(); s += (size_t) block)
            strip.process (in.data() + s, l.data() + s, r.data() + s, (int) std::min<size_t> ((size_t) block, in.size() - s));
        return l;
    }

    // a strummed open-position chord: six notes a few milliseconds apart, with their harmonics
    std::vector<float> strum (float peak, int n)
    {
        const double notes[6] = { 82.41, 110.0, 146.83, 196.0, 246.94, 329.63 };
        std::vector<float> v ((size_t) n, 0.0f);
        for (int s = 0; s < 6; ++s)
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr - s * 0.02;
                if (t < 0.0)
                    continue;
                double x = 0.0;
                for (int h = 1; h <= 6; ++h)
                    x += std::sin (6.283185307179586 * notes[s] * h * t) / h;
                v[(size_t) i] += (float) (x * std::exp (-t / 1.2));
            }
        float m = 0.0f;
        for (float x : v)
            m = std::max (m, std::abs (x));
        for (auto& x : v)
            x *= peak / m;
        return v;
    }

    void printRow (const char* name, const std::array<double, numBands>& out, const std::array<double, numBands>& in)
    {
        std::printf ("%-44s", name);
        for (int b = 0; b < numBands; ++b)
            std::printf (" %+6.1f", out[(size_t) b] - in[(size_t) b]);
        std::printf ("\n");
    }

    void toneReport()
    {
        std::printf ("level change per octave band, output minus input (dB)\n%-44s", "band centre (Hz):");
        for (int b = 0; b < numBands; ++b)
            std::printf (" %6.0f", std::sqrt (bandEdges[b] * bandEdges[b + 1]));
        std::printf ("\n");

        // the settings in use: plate reverb at 6.6, warmth 5.1, tone knobs flat, master 8.9
        const auto userSettings = [] (StripFixture& f) { f.warmth = 5.1f; f.reverb = 6.6f; f.master = 8.9f; };

        std::vector<float> noiseIn ((size_t) (sr * 5));
        for (auto& v : noiseIn)
            v = noise (0.03f);
        const auto noiseBands = bandLevels (noiseIn, 24000);

        { StripFixture f; userSettings (f); f.reverb = 0.0f; f.warmth = 0.0f;
          printRow ("quiet noise, warmth 0, no reverb", bandLevels (renderLeft (f, noiseIn), 24000), noiseBands); }
        { StripFixture f; userSettings (f); f.reverb = 0.0f;
          printRow ("quiet noise, warmth 5.1, no reverb", bandLevels (renderLeft (f, noiseIn), 24000), noiseBands); }
        { StripFixture f; userSettings (f);
          printRow ("quiet noise, warmth 5.1, plate reverb 6.6", bandLevels (renderLeft (f, noiseIn), 24000), noiseBands); }

        for (float peak : { 0.15f, 0.4f, 0.8f })
        {
            const auto chord = strum (peak, (int) (sr * 4));
            const auto in = bandLevels (chord, 12000);
            char name[96];

            { StripFixture f; userSettings (f); f.reverb = 0.0f; f.warmth = 0.0f;
              std::snprintf (name, sizeof name, "chord peak %.2f, warmth 0, no reverb", peak);
              printRow (name, bandLevels (renderLeft (f, chord), 12000), in); }
            { StripFixture f; userSettings (f); f.reverb = 0.0f;
              std::snprintf (name, sizeof name, "chord peak %.2f, warmth 5.1, no reverb", peak);
              printRow (name, bandLevels (renderLeft (f, chord), 12000), in); }
            { StripFixture f; userSettings (f);
              std::snprintf (name, sizeof name, "chord peak %.2f, warmth 5.1, plate 6.6", peak);
              printRow (name, bandLevels (renderLeft (f, chord), 12000), in); }
        }
    }
}

int main (int argc, char** argv)
{
    if (argc >= 2 && std::string (argv[1]) == "--tone")
    {
        toneReport();
        return 0;
    }

    // GuitarDriveTests --logo <output.png> [font name]: save the chrome artist logo as an image
    if (argc >= 3 && std::string (argv[1]) == "--logo")
    {
        juce::ScopedJuceInitialiser_GUI init;
        const auto image = Logo::render (1500, 700, argc >= 4 ? juce::String (argv[3]) : juce::String (Logo::defaultFont()));
        juce::File out (argv[2]);
        out.deleteFile();
        juce::FileOutputStream stream (out);
        const bool ok = stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
        std::printf ("%s %s\n", ok ? "wrote" : "FAILED to write", argv[2]);
        return ok ? 0 : 1;
    }

    testGate();
    testTransposer();
    testCompressor();
    testWah();
    testDrives();
    testEq();
    testPostFx();
    testMetronome();
    testReverbs();
    testStrip();
    testPresets();

    std::printf ("\n%d failure%s\n", failures, failures == 1 ? "" : "s");
    return failures;
}
