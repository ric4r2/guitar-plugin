#include "ChannelStrip.h"

namespace
{
    float gainToLinear (float knob)   { return juce::Decibels::decibelsToGain (juce::jmap (knob, 0.0f, 10.0f, -24.0f, 24.0f)); }
    float masterToLinear (float knob) { return knob < 0.05f ? 0.0f : juce::Decibels::decibelsToGain (juce::jmap (knob, 0.0f, 10.0f, -40.0f, 10.0f)); }
    float reverbToLinear (float knob) { return std::pow (knob / 10.0f, 1.5f); }
    float eqKnobToDb (float knob)     { return (knob - 5.0f) * 2.4f; }   // 5 = flat, +/-12 dB at the ends

    // Triode-style stage: a biased tanh is lopsided, which adds the even harmonics (2nd, 4th)
    // that make a tube amp sound warm. Normalised so small signals pass at unity gain.
    constexpr float tubeBias = 0.35f;
}

void ChannelStrip::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = maxBlockSize;

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 1 };
    for (auto* f : { &bassFilter, &midFilter, &trebleFilter })
        f->prepare (spec);
    lastBass = lastMid = lastTreble = -1.0f;
    updateToneStack();

    oversampling.initProcessing ((size_t) maxBlockSize);
    oversampling.reset();

    spring.prepare (sampleRate);

    gainSmooth.reset (sampleRate, 0.03);
    reverbSmooth.reset (sampleRate, 0.03);
    masterSmooth.reset (sampleRate, 0.03);
    warmthSmooth.reset (sampleRate * 4.0, 0.03);   // runs at the oversampled rate
    gainSmooth.setCurrentAndTargetValue (gainToLinear (params.gain->load()));
    reverbSmooth.setCurrentAndTargetValue (reverbToLinear (params.reverb->load()));
    masterSmooth.setCurrentAndTargetValue (masterToLinear (params.master->load()));
    warmthSmooth.setCurrentAndTargetValue (params.warmth->load());

    dcCoef = (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * 15.0 / sampleRate));
    dcState = toneState = 0.0f;
}

void ChannelStrip::reset()
{
    spring.reset();
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
        bassFilter.coefficients = Coeffs::makeLowShelf (sr, 120.0f, 0.7f, juce::Decibels::decibelsToGain (eqKnobToDb (bass)));
        lastBass = bass;
    }
    if (mid != lastMid)
    {
        midFilter.coefficients = Coeffs::makePeakFilter (sr, 750.0f, 0.8f, juce::Decibels::decibelsToGain (eqKnobToDb (mid)));
        lastMid = mid;
    }
    if (treble != lastTreble)
    {
        trebleFilter.coefficients = Coeffs::makeHighShelf (sr, 3200.0f, 0.7f, juce::Decibels::decibelsToGain (eqKnobToDb (treble)));
        lastTreble = treble;
    }
}

void ChannelStrip::process (const float* in, float* outL, float* outR, int n)
{
    const float warmth = params.warmth->load();
    gainSmooth.setTargetValue (gainToLinear (params.gain->load()));
    warmthSmooth.setTargetValue (warmth);
    reverbSmooth.setTargetValue (reverbToLinear (params.reverb->load()));
    masterSmooth.setTargetValue (masterToLinear (params.master->load()));

    for (int i = 0; i < n; ++i)
        outL[i] = in[i] * gainSmooth.getNextValue();

    // Tube stage at 4x oversampling so the added harmonics don't alias into fizz on fast attacks.
    {
        const float tanhBias = std::tanh (tubeBias);
        const float slopeAtRest = 1.0f - tanhBias * tanhBias;
        float* chans[1] = { outL };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);

        for (size_t start = 0; start < (size_t) n; start += (size_t) maxBlock)
        {
            auto sub = block.getSubBlock (start, std::min ((size_t) maxBlock, (size_t) n - start));
            auto up = oversampling.processSamplesUp (sub);
            float* d = up.getChannelPointer (0);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float k = warmthSmooth.getNextValue();
                const float drive = 0.4f + 0.3f * k;
                const float mix = std::min (1.0f, k * 0.5f);   // knob at 0 is fully clean
                const float x = d[i];
                const float shaped = (std::tanh (drive * x + tubeBias) - tanhBias) / (drive * slopeAtRest);
                d[i] = x + mix * (shaped - x);
            }
            oversampling.processSamplesDown (sub);
        }
    }

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
        float wl, wr;
        spring.process (dry, wl, wr);

        outL[i] = (dry + wet * wl) * master;
        outR[i] = (dry + wet * wr) * master;
    }
}
