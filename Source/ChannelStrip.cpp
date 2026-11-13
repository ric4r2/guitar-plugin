#include "ChannelStrip.h"

namespace
{
    float gainToLinear (float knob)   { return juce::Decibels::decibelsToGain (juce::jmap (knob, 0.0f, 10.0f, -24.0f, 24.0f)); }
    float masterToLinear (float knob) { return knob < 0.05f ? 0.0f : juce::Decibels::decibelsToGain (juce::jmap (knob, 0.0f, 10.0f, -40.0f, 10.0f)); }
    float reverbToLinear (float knob) { return std::pow (knob / 10.0f, 1.5f); }

    // Triode-style stage: a biased tanh is lopsided, which adds the even harmonics (2nd, 4th)
    // that make a tube amp sound warm. Normalised so small signals pass at unity gain.
    constexpr float tubeBias = 0.35f;
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

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
    for (auto* f : { &bassFilter, &midFilter, &trebleFilter })
        f->prepare (spec);
    lastBass = lastMid = lastTreble = -1.0f;
    updateToneStack();

    oversampling.initProcessing ((size_t) maxBlockSize);
    oversampling.reset();

    reverbDuck.prepare (sampleRate);
    spring.prepare (sampleRate);
    plate.prepare (sampleRate);

    gainSmooth.reset (sampleRate, 0.03);
    reverbSmooth.reset (sampleRate, 0.03);
    masterSmooth.reset (sampleRate, 0.03);
    reverbTypeSmooth.reset (sampleRate, 0.05);
    warmthSmooth.reset (sampleRate * 4.0, 0.03);   // runs at the oversampled rate
    gainSmooth.setCurrentAndTargetValue (gainToLinear (params.gain->load()));
    reverbSmooth.setCurrentAndTargetValue (reverbToLinear (params.reverb->load()));
    masterSmooth.setCurrentAndTargetValue (masterToLinear (params.master->load()));
    reverbTypeSmooth.setCurrentAndTargetValue (params.revType->load() > 0.5f ? 1.0f : 0.0f);
    warmthSmooth.setCurrentAndTargetValue (params.warmth->load());

    dcCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 15.0 / sampleRate));
    dcState = toneState = 0.0f;
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
    oversampling.reset();
    for (auto* f : { &bassFilter, &midFilter, &trebleFilter })
        f->reset();
    dcState = toneState = 0.0f;
}

void ChannelStrip::updateToneStack()
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    const float bass = params.bass->load(), mid = params.mid->load(), treble = params.treble->load();

    if (bass != lastBass)
    {
        bassFilter.coefficients = Coeffs::makeLowShelf (sr, 120.0f, 0.7f, fx::dbToGain (fx::knobDb (bass)));
        lastBass = bass;
    }
    if (mid != lastMid)
    {
        midFilter.coefficients = Coeffs::makePeakFilter (sr, 750.0f, 0.8f, fx::dbToGain (fx::knobDb (mid)));
        lastMid = mid;
    }
    if (treble != lastTreble)
    {
        trebleFilter.coefficients = Coeffs::makeHighShelf (sr, 3200.0f, 0.7f, fx::dbToGain (fx::knobDb (treble)));
        lastTreble = treble;
    }
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

    const float warmth = params.warmth->load();
    gainSmooth.setTargetValue (gainToLinear (params.gain->load()));
    warmthSmooth.setTargetValue (warmth);
    reverbSmooth.setTargetValue (reverbToLinear (params.reverb->load()));
    masterSmooth.setTargetValue (masterToLinear (params.master->load()));
    reverbTypeSmooth.setTargetValue (params.revType->load() > 0.5f ? 1.0f : 0.0f);

    for (int i = 0; i < n; ++i)
        outL[i] *= gainSmooth.getNextValue();

    // Tube stage at 4x oversampling so the added harmonics don't alias into fizz on fast attacks.
    fx::processOversampled (oversampling, outL, n, maxBlock, [&] (float* d, size_t count)
    {
        const float tanhBias = std::tanh (tubeBias);
        const float slopeAtRest = 1.0f - tanhBias * tanhBias;
        for (size_t i = 0; i < count; ++i)
        {
            const float k = warmthSmooth.getNextValue();
            const float drive = 0.4f + 0.3f * k;
            const float mix = std::min (1.0f, k * 0.5f);   // knob at 0 is fully clean
            const float x = d[i];
            const float shaped = (std::tanh (drive * x + tubeBias) - tanhBias) / (drive * slopeAtRest);
            d[i] = x + mix * (shaped - x);
        }
    });

    // Bias leaves a little DC, and tube stages soften the top end as they work harder.
    {
        const float cutoff = juce::jmap (warmth, 0.0f, 10.0f, (float) std::min (20000.0, sr * 0.45), 5500.0f);
        const float toneCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sr);
        for (int i = 0; i < n; ++i)
        {
            float y = outL[i];
            dcState += dcCoef * (y - dcState);
            y -= dcState;
            toneState += toneCoef * (y - toneState);
            outL[i] = toneState;
        }
    }

    updateToneStack();
    float* chans[1] = { outL };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    bassFilter.process (ctx);
    midFilter.process (ctx);
    trebleFilter.process (ctx);

    for (int i = 0; i < n; ++i)
    {
        const float dry = outL[i];
        const float wet = reverbSmooth.getNextValue();
        const float master = masterSmooth.getNextValue();
        const float type = reverbTypeSmooth.getNextValue();

        const float send = reverbDuck.process (dry);   // the pick attack stays out of the reverb
        float sl, sr2, pl, pr;
        spring.process (send, sl, sr2);
        plate.process (send, pl, pr);
        const float wl = pl + type * (sl - pl);   // type 0 = plate, 1 = spring
        const float wr = pr + type * (sr2 - pr);

        outL[i] = (dry + wet * wl) * master;
        outR[i] = (dry + wet * wr) * master;
    }
}
