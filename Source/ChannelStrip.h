#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AmpModel.h"
#include "Effects.h"
#include "Params.h"
#include "PlateReverb.h"
#include "SpringReverb.h"

// One complete guitar signal path, one per channel:
// gate -> transposer -> wah -> compressor -> tube screamer -> overdrive
//      -> amp (12AX7 -> tone stack -> volume -> 12AX7 -> power amp) -> reverb -> speaker -> master
//      -> equalizer -> phaser -> chorus -> tremolo -> delay -> digital reverb.
// Mono mode uses one strip; stereo mode runs two, one per input channel, each with its own settings.
class ChannelStrip
{
public:
    // pointers to the effects after the equalizer
    struct PostPtrs
    {
        const std::atomic<float>* phOn = nullptr;
        const std::atomic<float>* phRate = nullptr;
        const std::atomic<float>* phDepth = nullptr;
        const std::atomic<float>* phFb = nullptr;
        const std::atomic<float>* phMix = nullptr;
        const std::atomic<float>* chOn = nullptr;
        const std::atomic<float>* chRate = nullptr;
        const std::atomic<float>* chDepth = nullptr;
        const std::atomic<float>* chMix = nullptr;
        const std::atomic<float>* trOn = nullptr;
        const std::atomic<float>* trRate = nullptr;
        const std::atomic<float>* trDepth = nullptr;
        const std::atomic<float>* dlOn = nullptr;
        const std::atomic<float>* dlTime = nullptr;
        const std::atomic<float>* dlFb = nullptr;
        const std::atomic<float>* dlTone = nullptr;
        const std::atomic<float>* dlMix = nullptr;
        const std::atomic<float>* rvOn = nullptr;
        const std::atomic<float>* rvSize = nullptr;
        const std::atomic<float>* rvDamp = nullptr;
        const std::atomic<float>* rvMix = nullptr;
    };

    struct Params
    {
        // amp knobs
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
        const std::atomic<float>* ampType = nullptr;   // 0 = Deluxe Reverb, 1 = Twin Reverb

        // pedals before the amp
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

        // equalizer and the effects after it
        const std::atomic<float>* eqOn = nullptr;
        std::array<const std::atomic<float>*, ::Params::numEqBands> eq {};
        PostPtrs post;
    };

    void setParams (const Params& p) { params = p; }
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    int getLatencySamples() const { return (int) oversampling.getLatencyInSamples(); }

    // `in` may alias `outL`. Writes the finished stereo signal to outL and outR (n samples each).
    void process (const float* in, float* outL, float* outR, int n);

private:
    template <typename F>
    void runPedal (fx::PedalSwitch& sw, bool on, float* x, int n, F&& process);

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

    // the amp, modelled on its circuit (see AmpModel.h): all of it runs at 4x the sample rate except the speaker
    void applyAmpModel (int model);
    amp::TriodeStage v1a, v1b;
    amp::ToneStack toneStack;
    amp::PowerAmp powerAmp;
    amp::Speaker speakerL, speakerR;
    int ampModel = -1;
    float baseDrive = 0.125f, outputTrim = 1.0f;
    float coupleState = 0.0f, coupleCoef = 0.0f;               // coupling capacitor after V1B
    float sendHp1 = 0.0f, sendHp2 = 0.0f, sendHpCoef = 0.0f;   // keeps low-mids out of the reverb

    juce::dsp::Oversampling<float> oversampling { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };   // 4x

    fx::ReverbDuck reverbDuck;
    SpringReverb spring;
    PlateReverb plate;
    juce::SmoothedValue<float> volumeSmooth, driveSmooth, reverbSmooth, masterSmooth, reverbTypeSmooth;

    fx::GraphicEq eq;
    fx::PedalSwitch eqSwitch;
    fx::PostFx post;
    std::vector<float> eqDryL, eqDryR;
};
