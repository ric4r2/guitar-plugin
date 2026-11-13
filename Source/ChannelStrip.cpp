#include "ChannelStrip.h"

namespace
{
    // the amp's volume pot is a 1M audio-taper pot: about a tenth of the signal at half way, like the real one
    float volumeTaper (float knob)    { const float v = juce::jlimit (0.0f, 1.0f, knob / 10.0f); return std::pow (v, 3.3f); }
    float masterToLinear (float knob) { return knob < 0.05f ? 0.0f : juce::Decibels::decibelsToGain (juce::jmap (knob, 0.0f, 10.0f, -40.0f, 10.0f)); }
    float reverbToLinear (float knob) { return std::pow (knob / 10.0f, 1.5f); }
    // the recorded tank IRs are packed with Dwell 5 / Tone 5 at unit energy; this sets them level with the plate
    constexpr float springIRGain = 0.75f;
    // The first stage works on the guitar at a third of its level and makes it back up afterwards: clean notes keep
    // their sparkle instead of picking up the extra tones (about 1.2 % on two high strings) that smeared them.
    constexpr float firstStageIn = 1.0f / 3.0f;
    // with both amps playing, each comes in at this level (the two are closely related, so they add up louder)
    constexpr float bothGain = 0.6f;
    // WARMTH pushes the power tubes: 3 is the stock amp, each step is 1.5 dB more (or less) drive into them
    float warmthToDrive (float knob)  { return juce::Decibels::decibelsToGain ((knob - 3.0f) * 1.5f); }
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
    wahSwitch.prepare (sampleRate, params.wahOn->load() > 0.5f);
    compSwitch.prepare (sampleRate, params.compOn->load() > 0.5f);
    tsSwitch.prepare (sampleRate, params.tsOn->load() > 0.5f);
    odSwitch.prepare (sampleRate, params.odOn->load() > 0.5f);

    sendHpCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 300.0 / sampleRate));
    sendHp1 = sendHp2 = 0.0f;

    // AB763 preamp: 12AX7 with 100k plate loads at about 270 V. V1A has a 1.5k cathode resistor, V1B shares an 820 ohm
    // one with its neighbour, which is about 1.6k each.
    v1a.build (270.0, 100e3, 1.5e3, 12.0);
    v1b.build (270.0, 100e3, 1.64e3, 24.0);
    coupleCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 12.0 / (sampleRate * 4.0)));
    // speaker cabinets start with the built-in ones; recorded ones arrive through postIR()
    {
        const juce::dsp::ProcessSpec stereoSpec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        for (int m = 0; m < 2; ++m)
        {
            auto& c = cabinets[(size_t) m];
            c.prepare (stereoSpec);
            c.loadImpulseResponse (cab::build ((cab::Model) m, sampleRate), sampleRate,
                                   juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no,
                                   juce::dsp::Convolution::Normalise::no);
        }
        springConv.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
        useSpringIR = false;
        for (auto* b : { &sendBuf, &plateL, &plateR, &springL, &springR, &ampIn, &mixDry, &wetL, &wetR, &masterGain })
            b->assign ((size_t) maxBlockSize, 0.0f);
    }
    for (int k = 0; k < 2; ++k)
    {
        auto& a = amps[(size_t) k];
        a.oversampling.initProcessing ((size_t) maxBlockSize);
        a.powerAmp.prepare (sampleRate * 4.0);
        configureAmp (k);
        for (auto* b : { &a.buffer, &a.mixGain, &a.cabL, &a.cabR })
            b->assign ((size_t) maxBlockSize, 0.0f);
        a.volume.reset (sampleRate * 4.0, 0.03);   // these two run at the oversampled rate
        a.drive.reset (sampleRate * 4.0, 0.03);
        a.mix.reset (sampleRate, 0.04);
        a.volume.setCurrentAndTargetValue (volumeTaper (params.gain->load()));
        a.drive.setCurrentAndTargetValue (a.baseDrive * warmthToDrive (params.warmth->load()));
        resetAmp (k);
    }
    {
        const int sel = ampSelection();
        amps[0].mix.setCurrentAndTargetValue (sel == 1 ? 0.0f : sel == 2 ? bothGain : 1.0f);
        amps[1].mix.setCurrentAndTargetValue (sel == 0 ? 0.0f : sel == 2 ? bothGain : 1.0f);
    }
    ++prepareCount;

    reverbDuck.prepare (sampleRate);
    spring.prepare (sampleRate);
    plate.prepare (sampleRate);

    reverbSmooth.reset (sampleRate, 0.03);
    masterSmooth.reset (sampleRate, 0.03);
    reverbTypeSmooth.reset (sampleRate, 0.05);
    reverbSmooth.setCurrentAndTargetValue (reverbToLinear (params.reverb->load()));
    masterSmooth.setCurrentAndTargetValue (masterToLinear (params.master->load()));
    reverbTypeSmooth.setCurrentAndTargetValue (params.revType->load() > 0.5f ? 1.0f : 0.0f);
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
    return cabinets[0].getCurrentIRSize() > 0 && cabinets[1].getCurrentIRSize() > 0;
}

int ChannelStrip::ampSelection() const
{
    return params.ampType == nullptr ? 0 : juce::jlimit (0, 2, (int) std::lround (params.ampType->load()));
}

void ChannelStrip::configureAmp (int index)
{
    auto& a = amps[(size_t) index];
    const bool twoRock = index == 1;
    a.toneStack.prepare (sr * 4.0, twoRock ? amp::twoRock : amp::twinReverb);

    // Volts out of V1B that drive the power amp to full power. Both are big clean amps meant for pedals: the 85 W
    // Twin Reverb (four 6L6) and a 50-100 W Two-Rock with a stiff supply.
    a.baseDrive = 1.0f / 30.0f;
    a.powerAmp.setSag (twoRock ? 0.08f : 0.12f);
    // Two-Rock clean channels pad the signal between preamp stages (a resistor divider, often on a gain-structure
    // switch), so the second stage stays clean much longer. The Fender feeds its volume pot straight into V1B.
    a.preampPad = twoRock ? 0.4f : 1.0f;

    // Level match: a quiet note with every knob at 5 comes out at about the same level whichever amp is chosen.
    const double smallSignal = v1a.getSmallSignalGain() * a.toneStack.analogMagnitude (800.0) * volumeTaper (5.0f) * a.preampPad
                             * v1b.getSmallSignalGain() * a.baseDrive;
    a.outputTrim = (float) (0.6 / smallSignal);   // 0.6: the cabinet's presence lift makes it sound louder than the 800 Hz figure

    // Less bass into the second stage and the power tubes keeps chords from turning to mush: the Two-Rock cuts a bit
    // higher, as its tighter low end is part of its voice.
    a.tightG = (float) std::tan (juce::MathConstants<double>::pi * (twoRock ? 100.0 : 75.0) / (sr * 4.0));
}

void ChannelStrip::resetAmp (int index)
{
    auto& a = amps[(size_t) index];
    a.toneStack.reset();
    a.powerAmp.reset();
    a.oversampling.reset();
    a.coupleState = a.brightS = a.tightS = 0.0f;
    cabinets[(size_t) index].reset();
}

void ChannelStrip::reset()
{
    gate.reset();
    wah.reset();
    comp.reset();
    tubeScreamer.reset();
    overdrive.reset();
    reverbDuck.reset();
    spring.reset();
    plate.reset();
    post.reset();
    resetAmp (0);
    resetAmp (1);
    springConv.reset();
    sendHp1 = sendHp2 = 0.0f;
}

void ChannelStrip::process (const float* in, float* outL, float* outR, int n)
{
    if ((int) dryScratch.size() < n)
        dryScratch.resize ((size_t) n);
    if (in != outL)
        std::copy (in, in + n, outL);

    gate.process (outL, n, params.gate->load());
    shifter.process (outL, n, params.trans->load());

    runPedal (wahSwitch, params.wahOn->load() > 0.5f, outL, n, [&]
    {
        wah.process (outL, n, params.wahAuto->load() > 0.5f, params.wahPos->load(), params.wahSens->load(), params.wahQ->load());
    });
    runPedal (compSwitch, params.compOn->load() > 0.5f, outL, n, [&]
    {
        comp.process (outL, n, params.compSustain->load(), params.compAttack->load(), params.compLevel->load());
    });
    runPedal (tsSwitch, params.tsOn->load() > 0.5f, outL, n, [&]
    {
        tubeScreamer.process (outL, n, params.tsDrive->load(), params.tsTone->load(), params.tsLevel->load());
    });
    runPedal (odSwitch, params.odOn->load() > 0.5f, outL, n, [&]
    {
        overdrive.process (outL, n, params.odDrive->load(), params.odTone->load(), params.odLevel->load());
    });

    takeNewIRs();
    const int sel = ampSelection();
    amps[0].mix.setTargetValue (sel == 1 ? 0.0f : sel == 2 ? bothGain : 1.0f);
    amps[1].mix.setTargetValue (sel == 0 ? 0.0f : sel == 2 ? bothGain : 1.0f);
    reverbSmooth.setTargetValue (reverbToLinear (params.reverb->load()));
    masterSmooth.setTargetValue (masterToLinear (params.master->load()));
    reverbTypeSmooth.setTargetValue (params.revType->load() > 0.5f ? 1.0f : 0.0f);

    for (auto* b : { &sendBuf, &plateL, &plateR, &springL, &springR, &ampIn, &mixDry, &wetL, &wetR, &masterGain })
        if ((int) b->size() < n)
            b->resize ((size_t) n);
    std::copy (outL, outL + n, ampIn.begin());
    std::fill (mixDry.begin(), mixDry.begin() + n, 0.0f);

    // The amps, at 4x oversampling so the tube harmonics don't alias into fizz:
    // V1A -> tone stack -> volume pot -> V1B -> coupling cap -> phase inverter and power tubes.
    for (int k = 0; k < 2; ++k)
    {
        auto& a = amps[(size_t) k];
        const bool active = a.mix.getTargetValue() > 0.0f || a.mix.getCurrentValue() > 0.0f;
        if (! active)
        {
            a.running = false;
            continue;
        }
        if (! a.running)   // switched on: start from silence, not from where it stopped
        {
            resetAmp (k);
            a.running = true;
        }
        for (auto* b : { &a.buffer, &a.mixGain, &a.cabL, &a.cabR })
            if ((int) b->size() < n)
                b->resize ((size_t) n);

        const float taper = volumeTaper (params.gain->load());
        a.volume.setTargetValue (taper);
        {
            // AB763 bright cap: 47pF across the 1M volume pot. Treble skips the part of the pot above the wiper, so the
            // lower the volume, the more sparkle: H = t + (1 - t) * highpass, with time constant t (1 - t) * 1M * 47pF.
            const double tau = juce::jmax (1.0e-12, (double) taper * (1.0 - taper) * 1.0e6 * 47.0e-12);
            const double fc = juce::jmin (0.45 * sr * 4.0, 1.0 / (juce::MathConstants<double>::twoPi * tau));
            a.brightG = (float) std::tan (juce::MathConstants<double>::pi * fc / (sr * 4.0));
        }
        a.drive.setTargetValue (a.baseDrive * warmthToDrive (params.warmth->load()));
        a.toneStack.setControls (params.treble->load() / 10.0f, params.mid->load() / 10.0f, params.bass->load() / 10.0f);

        std::copy (ampIn.begin(), ampIn.begin() + n, a.buffer.begin());
        fx::processOversampled (a.oversampling, a.buffer.data(), n, maxBlock, [&] (float* d, size_t count)
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
                // WARMTH works the power tubes harder without making clean notes much louder
                const float drive = a.drive.getNextValue();
                d[i] = a.powerAmp.process (v, drive) * a.outputTrim * std::pow (a.baseDrive / drive, 0.75f);
            }
        });

        for (int i = 0; i < n; ++i)
        {
            const float g = a.mix.getNextValue();
            a.mixGain[(size_t) i] = g;
            mixDry[(size_t) i] += g * a.buffer[(size_t) i];
        }
    }

    for (int i = 0; i < n; ++i)
    {
        // like the real amp's reverb driver, the tank gets a thin signal (no low-mids, so the wash doesn't muddy the
        // tone), minus the pick attack
        const float dry = mixDry[(size_t) i];
        sendHp1 += sendHpCoef * (dry - sendHp1);
        const float s1 = dry - sendHp1;
        sendHp2 += sendHpCoef * (s1 - sendHp2);
        const float send = reverbDuck.process (s1 - sendHp2);
        sendBuf[(size_t) i] = send;
        plate.process (send, plateL[(size_t) i], plateR[(size_t) i]);
        if (! useSpringIR)
            spring.process (send, springL[(size_t) i], springR[(size_t) i]);
    }

    if (useSpringIR)
    {
        // the recorded Fender tank (a real tank is mono)
        std::copy (sendBuf.begin(), sendBuf.begin() + n, springL.begin());
        float* chans[1] = { springL.data() };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        springConv.process (juce::dsp::ProcessContextReplacing<float> (block));
        for (int i = 0; i < n; ++i)
            springR[(size_t) i] = springL[(size_t) i] *= springIRGain;
    }

    for (int i = 0; i < n; ++i)
    {
        const float wet = reverbSmooth.getNextValue();
        const float type = reverbTypeSmooth.getNextValue();
        wetL[(size_t) i] = wet * (plateL[(size_t) i] + type * (springL[(size_t) i] - plateL[(size_t) i]));   // type 0 = plate, 1 = spring
        wetR[(size_t) i] = wet * (plateR[(size_t) i] + type * (springR[(size_t) i] - plateR[(size_t) i]));
        masterGain[(size_t) i] = masterSmooth.getNextValue();
    }

    // each amp, reverb included, comes out of its own speaker cabinet; with both on, the two cabinets are mixed
    std::fill (outL, outL + n, 0.0f);
    std::fill (outR, outR + n, 0.0f);
    for (int k = 0; k < 2; ++k)
    {
        auto& a = amps[(size_t) k];
        if (! a.running)
            continue;
        for (int i = 0; i < n; ++i)
        {
            const float g = a.mixGain[(size_t) i] * masterGain[(size_t) i];
            a.cabL[(size_t) i] = g * (a.buffer[(size_t) i] + wetL[(size_t) i]);
            a.cabR[(size_t) i] = g * (a.buffer[(size_t) i] + wetR[(size_t) i]);
        }
        float* chans[2] = { a.cabL.data(), a.cabR.data() };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        cabinets[(size_t) k].process (juce::dsp::ProcessContextReplacing<float> (block));
        for (int i = 0; i < n; ++i)
        {
            outL[i] += a.cabL[(size_t) i];
            outR[i] += a.cabR[(size_t) i];
        }
    }

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
