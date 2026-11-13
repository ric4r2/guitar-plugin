#include "ChannelStrip.h"

namespace
{
    // the amp's volume pot is a 1M audio-taper pot: about a tenth of the signal at half way, like the real one
    float volumeTaper (float knob)    { const float v = juce::jlimit (0.0f, 1.0f, knob / 10.0f); return std::pow (v, 3.3f); }
    float masterToLinear (float knob) { return knob < 0.05f ? 0.0f : juce::Decibels::decibelsToGain (juce::jmap (knob, 0.0f, 10.0f, -40.0f, 10.0f)); }
    float reverbToLinear (float knob) { return std::pow (knob / 10.0f, 1.5f); }
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
    powerAmp.prepare (sampleRate * 4.0);
    coupleCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 12.0 / (sampleRate * 4.0)));
    coupleState = 0.0f;
    // speaker cabinets: a recorded impulse response when one was supplied, otherwise the built-in one
    {
        const juce::dsp::ProcessSpec stereoSpec { sampleRate, (juce::uint32) maxBlockSize, 2 };
        for (int m = 0; m < 2; ++m)
        {
            auto& c = cabinets[(size_t) m];
            c.prepare (stereoSpec);
            const bool custom = customIR[(size_t) m].getNumSamples() > 0;
            juce::AudioBuffer<float> ir;
            if (custom)
                ir.makeCopyOf (customIR[(size_t) m]);
            else
                ir = cab::build ((cab::Model) m, sampleRate);
            c.loadImpulseResponse (std::move (ir), custom ? customIRRate[(size_t) m] : sampleRate,
                                   juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no,
                                   juce::dsp::Convolution::Normalise::no);
        }
    }
    ampModel = -1;
    applyAmpModel (params.ampType != nullptr && params.ampType->load() > 0.5f ? 1 : 0);

    oversampling.initProcessing ((size_t) maxBlockSize);
    oversampling.reset();

    reverbDuck.prepare (sampleRate);
    spring.prepare (sampleRate);
    plate.prepare (sampleRate);

    reverbSmooth.reset (sampleRate, 0.03);
    masterSmooth.reset (sampleRate, 0.03);
    reverbTypeSmooth.reset (sampleRate, 0.05);
    volumeSmooth.reset (sampleRate * 4.0, 0.03);   // these two run at the oversampled rate
    driveSmooth.reset (sampleRate * 4.0, 0.03);
    reverbSmooth.setCurrentAndTargetValue (reverbToLinear (params.reverb->load()));
    masterSmooth.setCurrentAndTargetValue (masterToLinear (params.master->load()));
    reverbTypeSmooth.setCurrentAndTargetValue (params.revType->load() > 0.5f ? 1.0f : 0.0f);
    volumeSmooth.setCurrentAndTargetValue (volumeTaper (params.gain->load()) * preampPad);
    driveSmooth.setCurrentAndTargetValue (baseDrive * warmthToDrive (params.warmth->load()));
}

void ChannelStrip::setCabinetIR (int model, const juce::AudioBuffer<float>& ir, double irSampleRate)
{
    customIR[(size_t) model].makeCopyOf (ir);
    customIRRate[(size_t) model] = irSampleRate;
}

bool ChannelStrip::cabinetsReady() const
{
    return cabinets[0].getCurrentIRSize() > 0 && cabinets[1].getCurrentIRSize() > 0;
}

void ChannelStrip::applyAmpModel (int model)
{
    if (model == ampModel)
        return;
    ampModel = model;
    const bool twoRock = model == 1;

    toneStack.prepare (sr * 4.0, twoRock ? amp::twoRock : amp::deluxeReverb);
    cabinets[(size_t) model].reset();

    // Volts out of V1B that drive the power amp to full power. The 22 W Deluxe (two 6V6) runs out of clean headroom
    // early and its supply sags; a 50-100 W Two-Rock (6L6, big transformers) stays clean and stiff far louder.
    baseDrive = twoRock ? 1.0f / 30.0f : 1.0f / 11.0f;
    powerAmp.setSag (twoRock ? 0.08f : 0.18f);
    // Two-Rock clean channels pad the signal between preamp stages (a resistor divider, often on a gain-structure
    // switch), so the second stage stays clean much longer. The Fender feeds its volume pot straight into V1B.
    preampPad = twoRock ? 0.4f : 1.0f;

    // Level match: a quiet note with every knob at 5 comes out at about the same level whichever amp is chosen.
    const double smallSignal = v1a.getSmallSignalGain() * toneStack.analogMagnitude (800.0) * volumeTaper (5.0f) * preampPad
                             * v1b.getSmallSignalGain() * baseDrive;
    volumeSmooth.setCurrentAndTargetValue (volumeTaper (params.gain->load()) * preampPad);
    outputTrim = (float) (0.6 / smallSignal);   // 0.6: the cabinet's presence lift makes it sound louder than the 800 Hz figure
    driveSmooth.setCurrentAndTargetValue (baseDrive * warmthToDrive (params.warmth->load()));
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
    oversampling.reset();
    toneStack.reset();
    powerAmp.reset();
    for (auto& c : cabinets)
        c.reset();
    coupleState = sendHp1 = sendHp2 = 0.0f;
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

    applyAmpModel (params.ampType != nullptr && params.ampType->load() > 0.5f ? 1 : 0);
    volumeSmooth.setTargetValue (volumeTaper (params.gain->load()) * preampPad);
    driveSmooth.setTargetValue (baseDrive * warmthToDrive (params.warmth->load()));
    reverbSmooth.setTargetValue (reverbToLinear (params.reverb->load()));
    masterSmooth.setTargetValue (masterToLinear (params.master->load()));
    reverbTypeSmooth.setTargetValue (params.revType->load() > 0.5f ? 1.0f : 0.0f);
    toneStack.setControls (params.treble->load() / 10.0f, params.mid->load() / 10.0f, params.bass->load() / 10.0f);

    // The amp, at 4x oversampling so the tube harmonics don't alias into fizz:
    // V1A -> tone stack -> volume pot -> V1B -> coupling cap -> phase inverter and power tubes.
    fx::processOversampled (oversampling, outL, n, maxBlock, [&] (float* d, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            float v = v1a.process (d[i]);
            v = toneStack.process (v);
            v = v1b.process (v * volumeSmooth.getNextValue());
            coupleState += coupleCoef * (v - coupleState);
            v -= coupleState;
            // WARMTH works the power tubes harder without making clean notes much louder
            const float drive = driveSmooth.getNextValue();
            d[i] = powerAmp.process (v, drive) * outputTrim * std::pow (baseDrive / drive, 0.75f);
        }
    });

    for (int i = 0; i < n; ++i)
    {
        const float dry = outL[i];
        const float wet = reverbSmooth.getNextValue();
        const float master = masterSmooth.getNextValue();
        const float type = reverbTypeSmooth.getNextValue();

        // like the real amp's reverb driver, the tank gets a thin signal (no low-mids, so the wash doesn't muddy the
        // tone), minus the pick attack
        sendHp1 += sendHpCoef * (dry - sendHp1);
        const float s1 = dry - sendHp1;
        sendHp2 += sendHpCoef * (s1 - sendHp2);
        const float send = reverbDuck.process (s1 - sendHp2);
        float sl, sr2, pl, pr;
        spring.process (send, sl, sr2);
        plate.process (send, pl, pr);
        const float wl = pl + type * (sl - pl);   // type 0 = plate, 1 = spring
        const float wr = pr + type * (sr2 - pr);

        outL[i] = (dry + wet * wl) * master;
        outR[i] = (dry + wet * wr) * master;
    }

    // everything, reverb included, comes out of the speaker cabinet
    {
        float* chans[2] = { outL, outR };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        cabinets[(size_t) ampModel].process (ctx);
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
