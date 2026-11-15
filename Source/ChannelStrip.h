#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AmpModel.h"
#include "CabinetIR.h"
#include "Effects.h"
#include "NamCapture.h"
#include "Params.h"
#include "SpringReverb.h"

// The whole guitar signal path:
// gate -> transposer -> wah -> compressor -> tube screamer -> overdrive (TS9 captures when built in)
//      -> Fender spring reverb unit -> three amp heads in parallel, each into its own cabinet
//      -> equalizer -> phaser -> chorus -> tremolo -> delay -> digital reverb.
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

    // one amp head's controls
    struct HeadPtrs
    {
        const std::atomic<float>* on = nullptr;
        const std::atomic<float>* volume = nullptr;
        const std::atomic<float>* treble = nullptr;
        const std::atomic<float>* mid = nullptr;
        const std::atomic<float>* bass = nullptr;
        const std::atomic<float>* master = nullptr;
    };

    struct Params
    {
        const std::atomic<float>* gate = nullptr;
        const std::atomic<float>* trans = nullptr;
        const std::atomic<float>* reverb = nullptr;   // the spring reverb unit's MIX
        const std::atomic<float>* autoLevel = nullptr;

        std::array<HeadPtrs, ::Params::numHeads> heads {};

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

    ChannelStrip();
    ~ChannelStrip();

    void setParams (const Params& p) { params = p; }

    // Recorded impulse responses, handed over from another thread (picked up at the start of the next block).
    enum IRTarget { fenderCabinet = 0, twoRockCabinet = 1, dumbleCabinet = 2, springTank = 3, numIRTargets = 4 };
    void postIR (int target, juce::AudioBuffer<float>&& ir, double irSampleRate);
    void collectGarbage();   // call from the thread that posts: frees what the audio thread has finished with
    // counts prepare() calls: anything posted before the latest prepare() has to be posted again
    int getPrepareCount() const { return prepareCount.load(); }
    double getSampleRate() const { return sr; }
    int getMaxBlockSize() const { return maxBlock; }

    // NAM captures: the Dumble head; the TS9 plays the Tube Screamer and the SD-1 the Overdrive once they have a capture
    NamPlayer& dumblePlayer() { return dumble.player; }
    NamPlayer& ts9Player() { return ts9; }
    NamPlayer& sd1Player() { return sd1; }

    // the cabinets load on a background thread; true once all are in use
    bool cabinetsReady() const;
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    int getLatencySamples() const { return (int) circuit[0].oversampling.getLatencyInSamples(); }

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
    // pedal captures: DRIVE picks the capture (the loading thread does that), TONE tilts it, LEVEL sets its output
    NamPlayer ts9, sd1;
    juce::dsp::IIR::Filter<float> ts9Tone, sd1Tone;
    float lastTs9Tone = -1.0f, lastSd1Tone = -1.0f;
    void runPedalCapture (NamPlayer& player, juce::dsp::IIR::Filter<float>& tone, float& lastTone, float toneKnob, float levelKnob, float* x, int n);
    fx::PedalSwitch wahSwitch, compSwitch, tsSwitch, odSwitch;
    std::vector<float> dryScratch, levelRef;

    // Auto level: learns how loud the amps make the clean guitar, then keeps the compressor and drive pedals from
    // making the amps more than 6 dB louder than that, however many are on. Measured at the amps' output, so it follows
    // what you hear, whatever the amps and pedals do to the tone.
    float inPeak = 0.0f, outPeak = 0.0f, peakDecay = 0.0f;
    float cleanGain = 0.0f;   // amps' output peak / guitar peak, learned while no level pedal is on (0 = not yet)
    float levelGain = 1.0f, levelTarget = 1.0f, levelStep = 0.0f;
    bool levelPedalsWereOn = false;

    // Fender (0) and Two-Rock (1) heads, modelled on their circuits (AmpModel.h), at 4x the sample rate
    struct CircuitHead
    {
        amp::ToneStack toneStack;
        amp::PowerAmp powerAmp;
        juce::dsp::Oversampling<float> oversampling { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
        juce::SmoothedValue<float> volume, drive;
        float baseDrive = 1.0f / 30.0f, outputTrim = 1.0f, preampPad = 1.0f, coupleState = 0.0f;
        float brightG = 0.0f, brightS = 0.0f;   // 47pF bright cap across the volume pot
        float tightG = 0.0f, tightS = 0.0f;     // low cut in front of the second stage
    };
    std::array<CircuitHead, 2> circuit;
    void configureCircuit (int index);
    void runCircuit (int index, float* x, int n);

    // Dumble (2): a NAM capture, then its own treble / middle / bass
    struct NamHead
    {
        NamPlayer player;
        juce::dsp::IIR::Filter<float> bassF, midF, trebleF;
        float lastBass = -1.0f, lastMid = -1.0f, lastTreble = -1.0f;
        juce::SmoothedValue<float> volume;
    };
    NamHead dumble;
    void runDumble (float* x, int n);

    // per head: on/off fade, master level, its output buffer
    struct HeadMix
    {
        juce::SmoothedValue<float> gain;
        std::vector<float> buffer;
        bool running = false;
    };
    std::array<HeadMix, ::Params::numHeads> mix;
    void resetHead (int index);

    amp::TriodeStage v1a, v1b;
    float coupleCoef = 0.0f;   // coupling capacitor after V1B

    std::array<juce::dsp::Convolution, ::Params::numHeads> cabinets;   // each head's speaker cabinet
    juce::dsp::Convolution springConv { juce::dsp::Convolution::NonUniform { 512 } };   // recorded spring tank
    bool useSpringIR = false;
    struct PendingIR { juce::AudioBuffer<float> buffer; double rate = 0.0; };
    std::array<std::atomic<PendingIR*>, numIRTargets> inbox {}, outbox {};
    std::atomic<int> prepareCount { 0 };
    void takeNewIRs();

    // the spring reverb unit
    fx::ReverbDuck reverbDuck;
    SpringReverb spring;
    juce::SmoothedValue<float> reverbSmooth;
    float sendHp1 = 0.0f, sendHp2 = 0.0f, sendHpCoef = 0.0f;   // keeps low-mids out of the tank
    std::vector<float> sendBuf, wetBuf, spareR, ampIn;

    fx::GraphicEq eq;
    fx::PedalSwitch eqSwitch;
    fx::PostFx post;
    std::vector<float> eqDryL, eqDryR;
};
