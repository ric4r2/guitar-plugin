#include "Effects.h"

namespace fx
{
    namespace
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        float onePole (double hz, double rate) { return (float) (1.0 - std::exp (-6.283185307179586 * hz / rate)); }
    }

    // ------------------------------------------------------------ reverb send ducking

    void ReverbDuck::prepare (double sampleRate)
    {
        line.assign ((size_t) std::max (8, (int) (0.0012 * sampleRate)), 0.0f);
        const auto coef = [sampleRate] (double seconds) { return (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate))); };
        fastAtk = coef (0.0003);
        fastRel = coef (0.010);
        slowAtk = coef (0.020);
        slowRel = coef (0.200);
        gainAtk = coef (0.0005);
        gainRel = coef (0.040);
        reset();
    }

    void ReverbDuck::reset()
    {
        std::fill (line.begin(), line.end(), 0.0f);
        pos = 0;
        fast = slow = 0.0f;
        gain = 1.0f;
    }

    float ReverbDuck::process (float in)
    {
        const float a = std::abs (in);
        fast += (a > fast ? fastAtk : fastRel) * (a - fast);
        slow += (a > slow ? slowAtk : slowRel) * (a - slow);

        const float ratio = fast / (slow + 1.0e-4f);   // the floor keeps silence from triggering it
        const float excess = std::max (0.0f, ratio - 2.0f);
        const float target = 1.0f / (1.0f + 1.2f * excess);
        gain += (target < gain ? gainAtk : gainRel) * (target - gain);

        const float out = line[pos];
        line[pos] = in;
        if (++pos >= line.size())
            pos = 0;
        return out * gain;
    }

    // ------------------------------------------------------------ noise gate

    void NoiseGate::prepare (double sampleRate)
    {
        envRelease = (float) std::exp (-1.0 / (0.020 * sampleRate));
        openCoef = (float) (1.0 - std::exp (-1.0 / (0.002 * sampleRate)));    // opens in about 2 ms
        closeCoef = (float) (1.0 - std::exp (-1.0 / (0.120 * sampleRate)));   // closes gently so decays aren't chopped
        holdSamples = (int) (0.040 * sampleRate);
        reset();
    }

    void NoiseGate::reset()
    {
        env = 0.0f;
        gain = 1.0f;
        hold = 0;
        open = true;
    }

    void NoiseGate::process (float* x, int n, float knob)
    {
        if (knob < 0.05f)
        {
            for (int i = 0; i < n; ++i)
            {
                gain += openCoef * (1.0f - gain);
                x[i] *= gain;
            }
            open = true;
            return;
        }

        const float threshold = dbToGain (juce::jmap (knob, 0.0f, 10.0f, -75.0f, -30.0f));
        const float closeBelow = threshold * 0.7f;

        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]);
            env = a > env ? a : a + (env - a) * envRelease;

            if (env > threshold)
            {
                open = true;
                hold = holdSamples;
            }
            else if (env < closeBelow)
            {
                if (hold > 0)
                    --hold;
                else
                    open = false;
            }

            const float target = open ? 1.0f : 0.0f;
            gain += (target > gain ? openCoef : closeCoef) * (target - gain);
            x[i] *= gain;
        }
    }

    // ------------------------------------------------------------ transposer

    void PitchShifter::prepare (double sampleRate)
    {
        fadeLength = (int) (0.004 * sampleRate);
        searchRange = (int) (0.008 * sampleRate);   // covers a full period of the lowest guitar notes
        corrLength = fadeLength;
        jump = (int) (0.020 * sampleRate);
        minDelay = 2.0 * fadeLength + searchRange + 128.0;
        maxDelay = minDelay + jump;

        int size = 1;
        while ((double) size < maxDelay + searchRange + jump + 256.0)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        reference.assign ((size_t) corrLength, 0.0f);
        mask = size - 1;

        count = 0;
        headPos = -(minDelay + 0.5 * jump);   // start in the middle of the allowed delay range
        oldPos = headPos;
        fadeIndex = fadeLength;               // not fading

        semis.reset (sampleRate, 0.03);
        semis.setCurrentAndTargetValue (0.0f);
        active.reset (sampleRate, 0.02);
        active.setCurrentAndTargetValue (0.0f);
    }

    float PitchShifter::read (double pos) const
    {
        const double fl = std::floor (pos);
        const float f = (float) (pos - fl);
        const long long i = (long long) fl;
        const float a = buffer[(size_t) (i & mask)];
        const float b = buffer[(size_t) ((i + 1) & mask)];
        return a + f * (b - a);
    }

    // The read head has drifted to the edge of its delay range: jump it by one window, to the spot nearby
    // where the waveform best matches what the old head is about to play.
    void PitchShifter::startSplice (double ratio)
    {
        const double delay = (double) count - headPos;
        const double targetDelay = ratio > 1.0 ? delay + jump : delay - jump;

        for (int k = 0; k < corrLength; ++k)
            reference[(size_t) k] = read (headPos + k * ratio);

        double bestScore = -1.0e30, bestPos = (double) count - targetDelay;
        const double newest = (double) count - 2.0;
        const double oldest = (double) count - (double) (mask - 8);

        for (int s = -searchRange; s <= searchRange; ++s)
        {
            const double candidate = (double) count - (targetDelay + s);
            if (candidate < oldest || candidate + (corrLength - 1) * ratio > newest)
                continue;

            double dot = 0.0, energy = 0.0;
            for (int k = 0; k < corrLength; ++k)
            {
                const float c = read (candidate + k * ratio);
                dot += (double) reference[(size_t) k] * c;
                energy += (double) c * c;
            }

            const double score = dot / std::sqrt (energy + 1.0e-9);
            if (score > bestScore)
            {
                bestScore = score;
                bestPos = candidate;
            }
        }

        oldPos = headPos;
        headPos = bestPos;
        fadeIndex = 0;
    }

    void PitchShifter::process (float* x, int n, float semitones)
    {
        semis.setTargetValue (semitones);
        active.setTargetValue (std::abs (semitones) > 0.01f ? 1.0f : 0.0f);

        for (int i = 0; i < n; ++i)
        {
            const float in = x[i];
            buffer[(size_t) (count & mask)] = in;
            ++count;

            const double ratio = std::pow (2.0, (double) semis.getNextValue() / 12.0);
            headPos += ratio;
            const bool fading = fadeIndex < fadeLength;
            if (fading)
                oldPos += ratio;

            if (! fading && std::abs (ratio - 1.0) > 1.0e-3)
            {
                const double delay = (double) count - headPos;
                if ((ratio > 1.0 && delay < minDelay) || (ratio < 1.0 && delay > maxDelay))
                    startSplice (ratio);
            }

            float shifted;
            if (fadeIndex < fadeLength)
            {
                const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) fadeIndex / (float) fadeLength);
                shifted = (1.0f - w) * read (oldPos) + w * read (headPos);
                ++fadeIndex;
            }
            else
            {
                shifted = read (headPos);
            }

            x[i] = in + active.getNextValue() * (shifted - in);
        }
    }

    // ------------------------------------------------------------ wah

    void Wah::prepare (double sampleRate)
    {
        sr = sampleRate;
        envAttack = (float) std::exp (-1.0 / (0.005 * sampleRate));
        envRelease = (float) std::exp (-1.0 / (0.150 * sampleRate));
        posCoef = onePole (10.0, sampleRate);   // smooths scrolling and auto sweeps
        reset();
    }

    void Wah::reset()
    {
        ic1 = ic2 = 0.0f;
        env = 0.0f;
        pos = 0.5f;
    }

    void Wah::process (float* x, int n, bool autoMode, float posKnob, float sensKnob, float resKnob)
    {
        constexpr float fMin = 380.0f, fMax = 2200.0f;
        const float q = juce::jmap (resKnob, 0.0f, 10.0f, 2.0f, 9.0f);
        const float k = 1.0f / q;
        const float pos01 = posKnob / 10.0f;
        const float sens = 1.0f + 14.0f * sensKnob / 10.0f;
        const float maxFc = 0.45f * (float) sr;
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float in = x[i];
            const float a = std::abs (in);
            env = a > env ? a + (env - a) * envAttack : a + (env - a) * envRelease;

            const float target = autoMode ? juce::jlimit (0.0f, 1.0f, pos01 * 0.4f + env * sens) : pos01;
            pos += posCoef * (target - pos);

            if ((i & 7) == 0)   // topology-preserving state variable filter, retuned every 8 samples
            {
                const float fc = std::min (fMin * std::pow (fMax / fMin, pos), maxFc);
                const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
                a1 = 1.0f / (1.0f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }

            const float v3 = in - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;

            x[i] = 0.2f * in + 2.2f * k * v1;   // k * v1 is the band-pass with unity gain at its peak
        }
    }

    // ------------------------------------------------------------ compressor

    void Compressor::prepare (double sampleRate)
    {
        sr = sampleRate;
        releaseCoef = (float) std::exp (-1.0 / (0.180 * sampleRate));
        gr = 0.0f;
    }

    void Compressor::process (float* x, int n, float sustainKnob, float attackKnob, float levelKnob)
    {
        const float threshold = juce::jmap (sustainKnob, 0.0f, 10.0f, -8.0f, -40.0f);
        const float ratio = juce::jmap (sustainKnob, 0.0f, 10.0f, 2.0f, 12.0f);
        const float slope = 1.0f - 1.0f / ratio;
        const float attackMs = juce::jmap (attackKnob, 0.0f, 10.0f, 1.0f, 40.0f);
        const float attackCoef = (float) std::exp (-1.0 / (attackMs * 0.001 * sr));
        const float makeupDb = 0.45f * (-threshold) * slope;
        const float levelDb = knobDb (levelKnob);
        constexpr float knee = 6.0f;

        for (int i = 0; i < n; ++i)
        {
            const float levelNow = 20.0f * std::log10 (std::max (std::abs (x[i]), 1.0e-6f));
            const float over = levelNow - threshold;

            float target = 0.0f;
            if (over > knee * 0.5f)
                target = slope * over;
            else if (over > -knee * 0.5f)
                target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

            const float coef = target > gr ? attackCoef : releaseCoef;
            gr = coef * gr + (1.0f - coef) * target;
            x[i] *= dbToGain (makeupDb + levelDb - gr);
        }
    }

    // ------------------------------------------------------------ tube screamer

    void TubeScreamer::prepare (double sampleRate, int maxBlockSize)
    {
        maxBlock = maxBlockSize;
        os.initProcessing ((size_t) maxBlockSize);
        osRate = sampleRate * 2.0;
        lowCoef = onePole (720.0, osRate);   // the pedal's 720 Hz corner
        reset();
    }

    void TubeScreamer::reset()
    {
        os.reset();
        low = t1 = t2 = 0.0f;
    }

    void TubeScreamer::process (float* x, int n, float driveKnob, float toneKnob, float levelKnob)
    {
        const float d = driveKnob / 10.0f;
        const float stageGain = 1.0f + 100.0f * d * d;
        const float toneHz = 900.0f * std::pow (6.67f, toneKnob / 10.0f);   // 900 Hz .. 6 kHz
        const float toneCoef = onePole (toneHz, osRate);
        constexpr float diodeLimit = 0.3f;

        processOversampled (os, x, n, maxBlock, [&] (float* s, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
            {
                const float in = s[i];
                low += lowCoef * (in - low);
                const float highs = in - low;
                const float clipped = diodeLimit * std::tanh (stageGain * highs / diodeLimit);
                const float y = in + clipped;
                t1 += toneCoef * (y - t1);
                t2 += toneCoef * (t1 - t2);
                s[i] = t2;
            }
        });

        const float level = dbToGain (knobDb (levelKnob));
        for (int i = 0; i < n; ++i)
            x[i] *= level;
    }

    // ------------------------------------------------------------ overdrive

    void Overdrive::prepare (double sampleRate, int maxBlockSize)
    {
        maxBlock = maxBlockSize;
        os.initProcessing ((size_t) maxBlockSize);
        osRate = sampleRate * 2.0;
        hpCoef = onePole (60.0, osRate);
        dcCoef = onePole (15.0, osRate);
        reset();
    }

    void Overdrive::reset()
    {
        os.reset();
        hpLow = dc = t1 = t2 = 0.0f;
    }

    void Overdrive::process (float* x, int n, float driveKnob, float toneKnob, float levelKnob)
    {
        constexpr float bias = 0.18f;
        const float tanhBias = std::tanh (bias);
        const float d = driveKnob / 10.0f;
        const float g = 1.0f + 40.0f * d * d;
        const float toneHz = 1200.0f * std::pow (6.0f, toneKnob / 10.0f);   // 1.2 kHz .. 7.2 kHz
        const float toneCoef = onePole (toneHz, osRate);

        processOversampled (os, x, n, maxBlock, [&] (float* s, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
            {
                const float in = s[i];
                hpLow += hpCoef * (in - hpLow);
                const float shaped = 0.7f * (std::tanh (g * (in - hpLow) + bias) - tanhBias);
                dc += dcCoef * (shaped - dc);   // the bias leaves a little DC
                const float y = shaped - dc;
                t1 += toneCoef * (y - t1);
                t2 += toneCoef * (t1 - t2);
                s[i] = t2;
            }
        });

        const float level = dbToGain (knobDb (levelKnob));
        for (int i = 0; i < n; ++i)
            x[i] *= level;
    }

    // ------------------------------------------------------------ graphic EQ

    void GraphicEq::prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
        for (auto& channel : filters)
            for (int b = 0; b < Params::numEqBands; ++b)
            {
                channel[(size_t) b].prepare (spec);
                channel[(size_t) b].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, Params::eqFreqs[b], 1.0f, 1.0f);
            }
        last.fill (0.0f);
    }

    void GraphicEq::process (float* l, float* r, int n, const std::array<float, Params::numEqBands>& gainsDb)
    {
        for (int b = 0; b < Params::numEqBands; ++b)
        {
            if (gainsDb[(size_t) b] == last[(size_t) b])
                continue;

            const auto coefs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, Params::eqFreqs[b], 1.0f, dbToGain (gainsDb[(size_t) b]));
            filters[0][(size_t) b].coefficients = coefs;
            filters[1][(size_t) b].coefficients = coefs;
            last[(size_t) b] = gainsDb[(size_t) b];
        }

        float* chans[2] = { l, r };
        for (size_t c = 0; c < 2; ++c)
        {
            float* one[1] = { chans[c] };
            juce::dsp::AudioBlock<float> block (one, 1, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            for (auto& f : filters[c])
                f.process (ctx);
        }
    }

    // ------------------------------------------------------------ tremolo

    void Tremolo::prepare (double sampleRate)
    {
        sr = sampleRate;
        phase = 0.0f;
        depth.reset (sampleRate, 0.02);
        depth.setCurrentAndTargetValue (0.0f);
    }

    void Tremolo::process (float* l, float* r, int n, bool on, float rateKnob, float depthKnob)
    {
        depth.setTargetValue (on ? depthKnob / 10.0f : 0.0f);
        const float rate = 0.5f * std::pow (24.0f, rateKnob / 10.0f);   // 0.5 .. 12 Hz
        const float inc = rate / (float) sr;

        for (int i = 0; i < n; ++i)
        {
            const float lfo = 0.5f + 0.5f * std::sin (twoPi * phase);
            phase += inc;
            phase -= std::floor (phase);
            const float gain = 1.0f - depth.getNextValue() * lfo;
            l[i] *= gain;
            r[i] *= gain;
        }
    }

    // ------------------------------------------------------------ delay

    void DelayFx::prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sampleRate * 1.5) + 4;
        for (auto& b : buf)
            b.assign ((size_t) size, 0.0f);
        lp.fill (0.0f);
        writePos = 0;
        time.reset (sampleRate, 0.10);
        time.setCurrentAndTargetValue (380.0f);
        mix.reset (sampleRate, 0.02);
        mix.setCurrentAndTargetValue (0.0f);
    }

    void DelayFx::reset()
    {
        for (auto& b : buf)
            std::fill (b.begin(), b.end(), 0.0f);
        lp.fill (0.0f);
        writePos = 0;
    }

    void DelayFx::process (float* l, float* r, int n, bool on, float timeMs, float fbKnob, float toneKnob, float mixKnob)
    {
        time.setTargetValue (timeMs);
        mix.setTargetValue (on ? mixKnob / 10.0f : 0.0f);
        const float fb = fbKnob / 10.0f * 0.92f;
        const float toneCoef = onePole (1200.0 * std::pow (10.0, toneKnob / 10.0 * 0.9), sr);   // darker repeats at low settings
        float* io[2] = { l, r };

        for (int i = 0; i < n; ++i)
        {
            const float delaySamples = time.getNextValue() * 0.001f * (float) sr;
            const float m = mix.getNextValue();

            float readPos = (float) writePos - delaySamples;
            if (readPos < 0.0f)
                readPos += (float) size;
            const int i0 = (int) readPos;
            const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
            const float frac = readPos - (float) i0;

            for (size_t c = 0; c < 2; ++c)
            {
                const float delayed = buf[c][(size_t) i0] + frac * (buf[c][(size_t) i1] - buf[c][(size_t) i0]);
                lp[c] += toneCoef * (delayed - lp[c]);
                const float in = io[c][i];
                buf[c][(size_t) writePos] = in + fb * lp[c];
                io[c][i] = in + m * delayed;
            }
            writePos = writePos + 1 >= size ? 0 : writePos + 1;
        }
    }

    // ------------------------------------------------------------ post chain

    void PostFx::prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        maxBlock = maxBlockSize;
        const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        phaser.prepare (spec);
        phaser.reset();
        chorus.prepare (spec);
        chorus.reset();
        reverb.prepare (spec);
        reverb.reset();
        tremolo.prepare (sampleRate);
        delay.prepare (sampleRate);
        reverbMix.reset (sampleRate, 0.03);
        reverbMix.setCurrentAndTargetValue (0.0f);
        wetL.assign ((size_t) maxBlockSize, 0.0f);
        wetR.assign ((size_t) maxBlockSize, 0.0f);
    }

    void PostFx::reset()
    {
        phaser.reset();
        chorus.reset();
        reverb.reset();
        delay.reset();
    }

    void PostFx::process (float* l, float* r, int n, const PostParams& p)
    {
        for (int start = 0; start < n; start += maxBlock)
            processChunk (l + start, r + start, std::min (maxBlock, n - start), p);
    }

    void PostFx::processChunk (float* l, float* r, int n, const PostParams& p)
    {
        float* chans[2] = { l, r };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        phaser.setRate (0.1f * std::pow (40.0f, p.phRate / 10.0f));   // 0.1 .. 4 Hz
        phaser.setDepth (p.phDepth / 10.0f);
        phaser.setCentreFrequency (900.0f);
        phaser.setFeedback (p.phFb / 10.0f * 0.85f);
        phaser.setMix (p.phOn ? p.phMix / 10.0f : 0.0f);
        phaser.process (ctx);

        chorus.setRate (0.2f * std::pow (25.0f, p.chRate / 10.0f));   // 0.2 .. 5 Hz
        chorus.setDepth (p.chDepth / 10.0f * 0.8f);
        chorus.setCentreDelay (7.0f);
        chorus.setFeedback (0.0f);
        chorus.setMix (p.chOn ? p.chMix / 10.0f : 0.0f);
        chorus.process (ctx);

        tremolo.process (l, r, n, p.trOn, p.trRate, p.trDepth);
        delay.process (l, r, n, p.dlOn, p.dlTime, p.dlFb, p.dlTone, p.dlMix);

        // Digital reverb as a send: wet-only copy added to the dry signal.
        juce::Reverb::Parameters rp;
        rp.roomSize = p.rvSize / 10.0f;
        rp.damping = p.rvDamp / 10.0f;
        rp.wetLevel = 0.33f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        std::copy (l, l + n, wetL.begin());
        std::copy (r, r + n, wetR.begin());
        float* wet[2] = { wetL.data(), wetR.data() };
        juce::dsp::AudioBlock<float> wetBlock (wet, 2, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> wetCtx (wetBlock);
        reverb.process (wetCtx);

        reverbMix.setTargetValue (p.rvOn ? p.rvMix / 10.0f : 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const float m = reverbMix.getNextValue();
            l[i] += m * wetL[(size_t) i];
            r[i] += m * wetR[(size_t) i];
        }
    }
}
