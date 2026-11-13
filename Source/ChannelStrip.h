#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SpringReverb.h"

// One complete guitar signal path:
// gain -> tube warmth -> bass/mid/treble -> spring reverb -> master.
// Mono mode uses one strip; stereo mode runs two, one per input channel.
class ChannelStrip
{
public:
    struct Params
    {
        const std::atomic<float>* gain = nullptr;
        const std::atomic<float>* warmth = nullptr;
        const std::atomic<float>* bass = nullptr;
        const std::atomic<float>* mid = nullptr;
        const std::atomic<float>* treble = nullptr;
        const std::atomic<float>* reverb = nullptr;
        const std::atomic<float>* master = nullptr;
    };

    void setParams (const Params& p) { params = p; }
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    int getLatencySamples() const { return (int) oversampling.getLatencyInSamples(); }

    // `in` may alias `outL`. Writes dry plus spring reverb to outL and outR (n samples each).
    void process (const float* in, float* outL, float* outR, int n);

private:
    void updateToneStack();

    using Filter = juce::dsp::IIR::Filter<float>;

    Params params;
    double sr = 44100.0;
    int maxBlock = 512;

    Filter bassFilter, midFilter, trebleFilter;
    float lastBass = -1.0f, lastMid = -1.0f, lastTreble = -1.0f;

    juce::dsp::Oversampling<float> oversampling { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };   // 4x

    SpringReverb spring;
    juce::SmoothedValue<float> gainSmooth, warmthSmooth, reverbSmooth, masterSmooth;

    float dcState = 0.0f, dcCoef = 0.0f, toneState = 0.0f;
};
