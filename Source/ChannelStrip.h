#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Effects.h"
#include "PlateReverb.h"
#include "SpringReverb.h"

// One complete guitar signal path:
// gate -> transposer -> wah -> compressor -> tube screamer -> overdrive -> gain -> tube warmth -> bass/mid/treble -> reverb -> master.
// Mono mode uses one strip; stereo mode runs two, one per input channel.
class ChannelStrip
{
public:
    struct Params
    {
        // knobs of this channel
        const std::atomic<float>* gate = nullptr;
        const std::atomic<float>* trans = nullptr;
        const std::atomic<float>* gain = nullptr;
        const std::atomic<float>* warmth = nullptr;
        const std::atomic<float>* bass = nullptr;
        const std::atomic<float>* mid = nullptr;
        const std::atomic<float>* treble = nullptr;
        const std::atomic<float>* reverb = nullptr;
        const std::atomic<float>* master = nullptr;
        const std::atomic<float>* revType = nullptr;   // 0 = plate, 1 = spring

        // pedals, shared by both channels
        const std::atomic<float>* wahOn = nullptr;
        const std::atomic<float>* wahAuto = nullptr;
        const std::atomic<float>* wahPos = nullptr;
        const std::atomic<float>* wahSens = nullptr;
        const std::atomic<float>* wahQ = nullptr;
        const std::atomic<float>* compOn = nullptr;
        const std::atomic<float>* compSustain = nullptr;
        const std::atomic<float>* compAttack = nullptr;
        const std::atomic<float>* compLevel = nullptr;
        const std::atomic<float>* tsOn = nullptr;
        const std::atomic<float>* tsDrive = nullptr;
        const std::atomic<float>* tsTone = nullptr;
        const std::atomic<float>* tsLevel = nullptr;
        const std::atomic<float>* odOn = nullptr;
        const std::atomic<float>* odDrive = nullptr;
        const std::atomic<float>* odTone = nullptr;
        const std::atomic<float>* odLevel = nullptr;
    };

    void setParams (const Params& p) { params = p; }
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    int getLatencySamples() const { return (int) oversampling.getLatencyInSamples(); }

    // `in` may alias `outL`. Writes dry plus reverb to outL and outR (n samples each).
    void process (const float* in, float* outL, float* outR, int n);

private:
    template <typename F>
    void runPedal (fx::PedalSwitch& sw, bool on, float* x, int n, F&& process);
    void updateToneStack();

    using Filter = juce::dsp::IIR::Filter<float>;

    Params params;
    double sr = 44100.0;
    int maxBlock = 512;

    fx::NoiseGate gate;
    fx::PitchShifter shifter;
    fx::Wah wah;
    fx::Compressor comp;
    fx::TubeScreamer tubeScreamer;
    fx::Overdrive overdrive;
    fx::PedalSwitch wahSwitch, compSwitch, tsSwitch, odSwitch;
    std::vector<float> dryScratch;

    Filter bassFilter, midFilter, trebleFilter;
    float lastBass = -1.0f, lastMid = -1.0f, lastTreble = -1.0f;

    juce::dsp::Oversampling<float> oversampling { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };   // 4x

    fx::ReverbDuck reverbDuck;
    SpringReverb spring;
    PlateReverb plate;
    juce::SmoothedValue<float> gainSmooth, warmthSmooth, reverbSmooth, masterSmooth, reverbTypeSmooth;

    float dcState = 0.0f, dcCoef = 0.0f, toneState = 0.0f;
};
