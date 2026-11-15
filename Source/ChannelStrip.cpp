#include "ChannelStrip.h"

namespace
{
    // the amp's volume pot is a 1M audio-taper pot: about a tenth of the signal at half way, like the real one
    float volumeTaper (float knob)    { const float v = juce::jlimit (0.0f, 1.0f, knob / 10.0f); return std::pow (v, 3.3f); }
    float masterToLinear (float knob) { return knob < 0.05f ? 0.0f : juce::Decibels::decibelsToGain (juce::jmap (knob, 0.0f, 10.0f, -40.0f, 10.0f)); }
    float reverbToLinear (float knob) { return std::pow (knob / 10.0f, 1.5f); }
    float knobToDb (float knob, float dbPerStep) { return (knob - 5.0f) * dbPerStep; }
    // the recorded tank IRs are packed with Dwell 5 / Tone 5 at unit energy; this sets the wet level
    constexpr float springIRGain = 0.75f;
    // The first stage works on the guitar at a third of its level and makes it back up afterwards: clean notes keep
    // their sparkle instead of picking up the extra tones (about 1.2 % on two high strings) that smeared them.
    constexpr float firstStageIn = 1.0f / 3.0f;
    // auto level: pedals can push the amps up to 6 dB harder than the clean guitar (enough to drive them, like a boost)
    constexpr float maxPedalPush = 2.0f;
}

ChannelStrip::ChannelStrip()
{
    for (auto* box : { &inbox, &outbox })   // std::atomic is not zeroed for us before C++20
        for (auto& p : *box)
            p.store (nullptr);
}

ChannelStrip::~ChannelStrip()
{
    for (auto* box : { &inbox, &outbox })
        for (auto& p : *box)
            delete p.load();
}

template <typename F>
void ChannelStrip::runPedal (fx::PedalSwitch& sw, bool on, float* x, int n, F&& process)
{
    sw.setOn (on);
    if (! sw.needsProcessing())
        return;

    std::copy (x, x + n, dryScratch.begin());
    process();
    sw.blend (dryScratch.data(), x, n);
}

void ChannelStrip::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = maxBlockSize;
    dryScratch.assign ((size_t) maxBlockSize, 0.0f);
    levelRef.assign ((size_t) maxBlockSize, 0.0f);
    inPeak = outPeak = cleanGain = 0.0f;
    levelGain = levelTarget = 1.0f;
    levelPedalsWereOn = false;
    peakDecay = (float) std::exp (-1.0 / (1.0 * sampleRate));             // note peaks, held for about a second
    levelStep = (float) (1.0 - std::exp (-1.0 / (0.08 * sampleRate)));    // gain moves over about 80 ms
    eqDryL.assign ((size_t) maxBlockSize, 0.0f);
    eqDryR.assign ((size_t) maxBlockSize, 0.0f);
    eq.prepare (sampleRate, maxBlockSize);
    eqSwitch.prepare (sampleRate, params.eqOn->load() > 0.5f);
    post.prepare (sampleRate, maxBlockSize);

    gate.prepare (sampleRate);
    shifter.prepare (sampleRate);
    wah.prepare (sampleRate);
    comp.prepare (sampleRate);
    tubeScreamer.prepare (sampleRate, maxBlockSize);
    overdrive.prepare (sampleRate, maxBlockSize);
    for (auto* p : { &ts9, &sd1 })
        p->prepare (sampleRate, maxBlockSize);
    for (auto* f : { &ts9Tone, &sd1Tone })
        f->prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
    lastTs9Tone = lastSd1Tone = -1.0f;
    wahSwitch.prepare (sampleRate, params.wahOn->load() > 0.5f);
    compSwitch.prepare (sampleRate, params.compOn->load() > 0.5f);
    tsSwitch.prepare (sampleRate, params.tsOn->load() > 0.5f);
    odSwitch.prepare (sampleRate, params.odOn->load() > 0.5f);

    // spring reverb unit
    sendHpCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 300.0 / sampleRate));
    sendHp1 = sendHp2 = 0.0f;
    reverbDuck.prepare (sampleRate);
    spring.prepare (sampleRate);
    springConv.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
    useSpringIR = false;
    reverbSmooth.reset (sampleRate, 0.03);
    reverbSmooth.setCurrentAndTargetValue (reverbToLinear (params.reverb->load()));
    for (auto* b : { &sendBuf, &wetBuf, &spareR, &ampIn })
        b->assign ((size_t) maxBlockSize, 0.0f);

    // AB763 preamp: 12AX7 with 100k plate loads at about 270 V. V1A has a 1.5k cathode resistor, V1B shares an 820 ohm
    // one with its neighbour, which is about 1.6k each.
    v1a.build (270.0, 100e3, 1.5e3, 12.0);
    v1b.build (270.0, 100e3, 1.64e3, 24.0);
    coupleCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 12.0 / (sampleRate * 4.0)));
    for (int k = 0; k < 2; ++k)
    {
        auto& c = circuit[(size_t) k];
        c.oversampling.initProcessing ((size_t) maxBlockSize);
        c.powerAmp.prepare (sampleRate * 4.0);
        configureCircuit (k);
        c.volume.reset (sampleRate * 4.0, 0.03);
        c.volume.setCurrentAndTargetValue (volumeTaper (params.heads[(size_t) k].volume->load()));
    }

    dumble.player.prepare (sampleRate, maxBlockSize);
    for (auto* f : { &dumble.bassF, &dumble.midF, &dumble.trebleF })
        f->prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
    dumble.lastBass = dumble.lastMid = dumble.lastTreble = -1.0f;
    dumble.volume.reset (sampleRate, 0.03);
    dumble.volume.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (knobToDb (params.heads[2].volume->load(), 2.4f)));

    // cabinets start with the built-in ones (the Dumble plays through the Two-Rock's 2x12); recorded ones come later
    for (int h = 0; h < ::Params::numHeads; ++h)
    {
        auto& c = cabinets[(size_t) h];
        c.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
        c.loadImpulseResponse (cab::build (h == 0 ? cab::fender : cab::twoRock, sampleRate), sampleRate,
                               juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no,
                               juce::dsp::Convolution::Normalise::no);
        auto& m = mix[(size_t) h];
        m.buffer.assign ((size_t) maxBlockSize, 0.0f);
        m.gain.reset (sampleRate, 0.04);
        m.gain.setCurrentAndTargetValue (0.0f);
        m.running = false;
    }
    ++prepareCount;
}

void ChannelStrip::configureCircuit (int index)
{
    auto& a = circuit[(size_t) index];
    const bool twoRock = index == 1;
    a.toneStack.prepare (sr * 4.0, twoRock ? amp::twoRock : amp::twinReverb);

    // Volts out of V1B that drive the power amp to full power. Both are big clean amps meant for pedals: the 85 W
    // Twin Reverb (four 6L6) and a 50-100 W Two-Rock with a stiff supply.
    a.baseDrive = 1.0f / 30.0f;
    a.powerAmp.setSag (twoRock ? 0.08f : 0.12f);
    // Two-Rock clean channels pad the signal between preamp stages (a resistor divider, often on a gain-structure
    // switch), so the second stage stays clean much longer. The Fender feeds its volume pot straight into V1B.
    a.preampPad = twoRock ? 0.4f : 1.0f;

    // Level match: a quiet note with every knob at 5 comes out at about the same level whichever amp it goes through.
    const double smallSignal = v1a.getSmallSignalGain() * a.toneStack.analogMagnitude (800.0) * volumeTaper (5.0f) * a.preampPad
                             * v1b.getSmallSignalGain() * a.baseDrive;
    a.outputTrim = (float) (0.6 / smallSignal);   // 0.6: the cabinet's presence lift makes it sound louder than the 800 Hz figure

    // Less bass into the second stage and the power tubes keeps chords from turning to mush: the Two-Rock cuts a bit
    // higher, as its tighter low end is part of its voice.
    a.tightG = (float) std::tan (juce::MathConstants<double>::pi * (twoRock ? 100.0 : 75.0) / (sr * 4.0));
}

void ChannelStrip::resetHead (int index)
{
    if (index < 2)
    {
        auto& a = circuit[(size_t) index];
        a.toneStack.reset();
        a.powerAmp.reset();
        a.oversampling.reset();
        a.coupleState = a.brightS = a.tightS = 0.0f;
    }
    else
    {
        for (auto* f : { &dumble.bassF, &dumble.midF, &dumble.trebleF })
            f->reset();
    }
    cabinets[(size_t) index].reset();
}

void ChannelStrip::postIR (int target, juce::AudioBuffer<float>&& ir, double irSampleRate)
{
    auto* p = new PendingIR { std::move (ir), irSampleRate };
    delete inbox[(size_t) target].exchange (p);   // an older one the audio thread never took
}

void ChannelStrip::collectGarbage()
{
    for (auto& o : outbox)
        delete o.exchange (nullptr);
    dumble.player.collectGarbage();
    ts9.collectGarbage();
    sd1.collectGarbage();
}

void ChannelStrip::takeNewIRs()
{
    for (int t = 0; t < numIRTargets; ++t)
    {
        if (outbox[(size_t) t].load() != nullptr)
            continue;   // the last one hasn't been freed yet: take this one next block
        auto* p = inbox[(size_t) t].exchange (nullptr);
        if (p == nullptr)
            continue;

        auto& conv = t == springTank ? springConv : cabinets[(size_t) t];
        conv.loadImpulseResponse (std::move (p->buffer), p->rate, juce::dsp::Convolution::Stereo::no,
                                  juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
        if (t == springTank)
            useSpringIR = true;
        outbox[(size_t) t].store (p);   // freed by collectGarbage(), never here on the audio thread
    }
}

bool ChannelStrip::cabinetsReady() const
{
    for (const auto& c : cabinets)
        if (c.getCurrentIRSize() <= 0)
            return false;
    return true;
}

void ChannelStrip::reset()
{
    gate.reset();
    wah.reset();
    comp.reset();
    tubeScreamer.reset();
    overdrive.reset();
    ts9.reset();
    sd1.reset();
    ts9Tone.reset();
    sd1Tone.reset();
    reverbDuck.reset();
    spring.reset();
    springConv.reset();
    post.reset();
    for (int h = 0; h < ::Params::numHeads; ++h)
        resetHead (h);
    dumble.player.reset();
    sendHp1 = sendHp2 = 0.0f;
}

void ChannelStrip::runCircuit (int index, float* x, int n)
{
    auto& a = circuit[(size_t) index];
    const auto& hp = params.heads[(size_t) index];
    const float taper = volumeTaper (hp.volume->load());
    a.volume.setTargetValue (taper);
    a.toneStack.setControls (hp.treble->load() / 10.0f, hp.mid->load() / 10.0f, hp.bass->load() / 10.0f);
    {
        // AB763 bright cap: 47pF across the 1M volume pot. Treble skips the part of the pot above the wiper, so the
        // lower the volume, the more sparkle: H = t + (1 - t) * highpass, with time constant t (1 - t) * 1M * 47pF.
        const double tau = juce::jmax (1.0e-12, (double) taper * (1.0 - taper) * 1.0e6 * 47.0e-12);
        const double fc = juce::jmin (0.45 * sr * 4.0, 1.0 / (juce::MathConstants<double>::twoPi * tau));
        a.brightG = (float) std::tan (juce::MathConstants<double>::pi * fc / (sr * 4.0));
    }

    // V1A -> tone stack -> volume pot -> V1B -> coupling cap -> phase inverter and power tubes, at 4x oversampling
    fx::processOversampled (a.oversampling, x, n, maxBlock, [&] (float* d, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            float v = v1a.process (d[i] * firstStageIn) / firstStageIn;
            v = a.toneStack.process (v);

            const float t = a.volume.getNextValue();
            const float bw = (v - a.brightS) * a.brightG / (1.0f + a.brightG);
            const float blp = bw + a.brightS;
            a.brightS = blp + bw;
            v = (t * v + (1.0f - t) * (v - blp)) * a.preampPad;   // volume pot with its bright cap

            const float tw = (v - a.tightS) * a.tightG / (1.0f + a.tightG);
            const float tlp = tw + a.tightS;
            a.tightS = tlp + tw;
            v = v1b.process (v - tlp);

            a.coupleState += coupleCoef * (v - a.coupleState);
            v -= a.coupleState;
            d[i] = a.powerAmp.process (v, a.baseDrive) * a.outputTrim;
        }
    });
}

void ChannelStrip::runPedalCapture (NamPlayer& player, juce::dsp::IIR::Filter<float>& tone, float& lastTone,
                                    float toneKnob, float levelKnob, float* x, int n)
{
    player.process (x, n);
    if (toneKnob != lastTone)   // 5 = as captured
    {
        tone.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 1500.0, 0.6f,
                                juce::Decibels::decibelsToGain (knobToDb (toneKnob, 1.6f)));
        lastTone = toneKnob;
    }
    float* chans[1] = { x };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    tone.process (juce::dsp::ProcessContextReplacing<float> (block));
    juce::FloatVectorOperations::multiply (x, juce::Decibels::decibelsToGain (knobToDb (levelKnob, 2.4f)), n);
}

void ChannelStrip::runDumble (float* x, int n)
{
    const auto& hp = params.heads[2];
    dumble.volume.setTargetValue (juce::Decibels::decibelsToGain (knobToDb (hp.volume->load(), 2.4f)));   // +-12 dB of drive
    for (int i = 0; i < n; ++i)
        x[i] *= dumble.volume.getNextValue();

    dumble.player.process (x, n);

    // the capture holds the amp's own tone settings; these shape it further (5 = as captured)
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    const float bass = hp.bass->load(), mid = hp.mid->load(), treble = hp.treble->load();
    if (bass != dumble.lastBass)
        dumble.bassF.coefficients = Coeffs::makeLowShelf (sr, 120.0, 0.7f, juce::Decibels::decibelsToGain (knobToDb (bass, 2.0f)));
    if (mid != dumble.lastMid)
        dumble.midF.coefficients = Coeffs::makePeakFilter (sr, 650.0, 0.7f, juce::Decibels::decibelsToGain (knobToDb (mid, 2.0f)));
    if (treble != dumble.lastTreble)
        dumble.trebleF.coefficients = Coeffs::makeHighShelf (sr, 3200.0, 0.7f, juce::Decibels::decibelsToGain (knobToDb (treble, 2.0f)));
    dumble.lastBass = bass; dumble.lastMid = mid; dumble.lastTreble = treble;

    float* chans[1] = { x };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    dumble.bassF.process (ctx);
    dumble.midF.process (ctx);
    dumble.trebleF.process (ctx);
}

void ChannelStrip::process (const float* in, float* outL, float* outR, int n)
{
    if ((int) dryScratch.size() < n)
        dryScratch.resize ((size_t) n);
    if ((int) levelRef.size() < n)
        levelRef.resize ((size_t) n);
    for (auto* b : { &sendBuf, &wetBuf, &spareR, &ampIn })
        if ((int) b->size() < n)
            b->resize ((size_t) n);
    if (in != outL)
        std::copy (in, in + n, outL);

    takeNewIRs();
    dumble.player.update();
    ts9.update();
    sd1.update();

    gate.process (outL, n, params.gate->load());
    shifter.process (outL, n, params.trans->load());

    runPedal (wahSwitch, params.wahOn->load() > 0.5f, outL, n, [&]
    {
        wah.process (outL, n, params.wahAuto->load() > 0.5f, params.wahPos->load(), params.wahSens->load(), params.wahQ->load());
    });
    std::copy (outL, outL + n, levelRef.begin());   // the guitar before the compressor and drive pedals
    runPedal (compSwitch, params.compOn->load() > 0.5f, outL, n, [&]
    {
        comp.process (outL, n, params.compSustain->load(), params.compAttack->load(), params.compLevel->load());
    });
    runPedal (tsSwitch, params.tsOn->load() > 0.5f, outL, n, [&]
    {
        if (ts9.hasModel())   // the TS9 captures
            runPedalCapture (ts9, ts9Tone, lastTs9Tone, params.tsTone->load(), params.tsLevel->load(), outL, n);
        else                  // the TS808 circuit
            tubeScreamer.process (outL, n, params.tsDrive->load(), params.tsTone->load(), params.tsLevel->load());
    });
    runPedal (odSwitch, params.odOn->load() > 0.5f, outL, n, [&]
    {
        if (sd1.hasModel())   // the Boss SD-1 captures
            runPedalCapture (sd1, sd1Tone, lastSd1Tone, params.odTone->load(), params.odLevel->load(), outL, n);
        else
            overdrive.process (outL, n, params.odDrive->load(), params.odTone->load(), params.odLevel->load());
    });

    // Auto level, part 1: the pedals' output follows the gain worked out at the end of the last block
    const bool levelPedalsOn = params.compOn->load() > 0.5f || params.tsOn->load() > 0.5f || params.odOn->load() > 0.5f;
    const bool autoLevelOn = levelPedalsOn && (params.autoLevel == nullptr || params.autoLevel->load() > 0.5f);
    if (! autoLevelOn)
        levelTarget = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        inPeak = juce::jmax (std::abs (levelRef[(size_t) i]), inPeak * peakDecay);
        levelGain += levelStep * (levelTarget - levelGain);
        outL[i] *= levelGain;
    }

    // Fender spring reverb unit in front of the amps: like the real one, the tank gets a thin signal (no low-mids, so
    // the wash doesn't muddy the tone), minus the pick attack, and its output is mixed with the dry guitar
    reverbSmooth.setTargetValue (reverbToLinear (params.reverb->load()));
    for (int i = 0; i < n; ++i)
    {
        const float dry = outL[i];
        sendHp1 += sendHpCoef * (dry - sendHp1);
        const float s1 = dry - sendHp1;
        sendHp2 += sendHpCoef * (s1 - sendHp2);
        const float send = reverbDuck.process (s1 - sendHp2);
        sendBuf[(size_t) i] = send;
        if (! useSpringIR)
            spring.process (send, wetBuf[(size_t) i], spareR[(size_t) i]);
    }
    if (useSpringIR)
    {
        std::copy (sendBuf.begin(), sendBuf.begin() + n, wetBuf.begin());
        float* chans[1] = { wetBuf.data() };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        springConv.process (juce::dsp::ProcessContextReplacing<float> (block));
        juce::FloatVectorOperations::multiply (wetBuf.data(), springIRGain, n);
    }
    for (int i = 0; i < n; ++i)
        ampIn[(size_t) i] = outL[i] + reverbSmooth.getNextValue() * wetBuf[(size_t) i];

    // The three heads in parallel, each into its own cabinet. Turning heads on or off keeps the loudness about the same.
    int playing = 0;
    for (int h = 0; h < ::Params::numHeads; ++h)
        if (params.heads[(size_t) h].on->load() > 0.5f && (h < 2 || dumble.player.hasModel()))
            ++playing;
    const float share = 1.0f / std::sqrt ((float) juce::jmax (1, playing));

    std::fill (outL, outL + n, 0.0f);
    for (int h = 0; h < ::Params::numHeads; ++h)
    {
        auto& m = mix[(size_t) h];
        const auto& hp = params.heads[(size_t) h];
        const bool on = hp.on->load() > 0.5f && (h < 2 || dumble.player.hasModel());
        m.gain.setTargetValue (on ? masterToLinear (hp.master->load()) * share : 0.0f);
        if (! on && m.gain.getCurrentValue() <= 0.0f && ! m.gain.isSmoothing())
        {
            m.running = false;
            continue;
        }
        if (! m.running)   // switched on: start from silence, not from where it stopped
        {
            resetHead (h);
            m.running = true;
        }
        if ((int) m.buffer.size() < n)
            m.buffer.resize ((size_t) n);

        std::copy (ampIn.begin(), ampIn.begin() + n, m.buffer.begin());
        if (h < 2)
            runCircuit (h, m.buffer.data(), n);
        else
            runDumble (m.buffer.data(), n);

        float* chans[1] = { m.buffer.data() };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        cabinets[(size_t) h].process (juce::dsp::ProcessContextReplacing<float> (block));
        for (int i = 0; i < n; ++i)
            outL[i] += m.gain.getNextValue() * m.buffer[(size_t) i];
    }
    std::copy (outL, outL + n, outR);

    // Auto level, part 2: compare how hard notes hit at the amps' output with the clean reference
    for (int i = 0; i < n; ++i)
        outPeak = juce::jmax (std::abs (outL[i]), outPeak * peakDecay);
    if (inPeak > 1.0e-3f)   // only while playing
    {
        const float gainNow = outPeak / inPeak;
        if (! levelPedalsOn)
        {
            // clean: learn the amps' gain (fast at first, then slowly as settings change)
            cleanGain = cleanGain <= 0.0f ? gainNow : cleanGain + 0.02f * (gainNow - cleanGain);
        }
        else if (autoLevelOn && cleanGain > 0.0f)
        {
            // too loud: turn the pedals down; quieter than allowed: let them back up (never above unity)
            const float allowed = maxPedalPush * cleanGain;
            const float ratio = allowed / juce::jmax (1.0e-6f, gainNow);
            levelTarget = juce::jlimit (0.001f, 1.0f, levelTarget * std::pow (ratio, 0.25f));
        }
    }
    levelPedalsWereOn = levelPedalsOn;

    // equalizer, then the effects after it
    {
        if ((int) eqDryL.size() < n)
        {
            eqDryL.resize ((size_t) n);
            eqDryR.resize ((size_t) n);
        }

        std::array<float, ::Params::numEqBands> gains;
        for (size_t b = 0; b < gains.size(); ++b)
            gains[b] = params.eq[b]->load();

        eqSwitch.setOn (params.eqOn->load() > 0.5f);
        if (eqSwitch.needsProcessing())
        {
            std::copy (outL, outL + n, eqDryL.begin());
            std::copy (outR, outR + n, eqDryR.begin());
            eq.process (outL, outR, n, gains);
            eqSwitch.blend (eqDryL.data(), eqDryR.data(), outL, outR, n);
        }
    }

    {
        const auto& q = params.post;
        fx::PostParams pp;
        pp.phOn = q.phOn->load() > 0.5f;   pp.phRate = q.phRate->load();   pp.phDepth = q.phDepth->load();
        pp.phFb = q.phFb->load();   pp.phMix = q.phMix->load();
        pp.chOn = q.chOn->load() > 0.5f;   pp.chRate = q.chRate->load();   pp.chDepth = q.chDepth->load();
        pp.chMix = q.chMix->load();
        pp.trOn = q.trOn->load() > 0.5f;   pp.trRate = q.trRate->load();   pp.trDepth = q.trDepth->load();
        pp.dlOn = q.dlOn->load() > 0.5f;   pp.dlTime = q.dlTime->load();   pp.dlFb = q.dlFb->load();
        pp.dlTone = q.dlTone->load();   pp.dlMix = q.dlMix->load();
        pp.rvOn = q.rvOn->load() > 0.5f;   pp.rvSize = q.rvSize->load();   pp.rvDamp = q.rvDamp->load();
        pp.rvMix = q.rvMix->load();
        post.process (outL, outR, n, pp);
    }
}
