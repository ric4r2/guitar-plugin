#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include "Params.h"

// Guitar effects. Every process() works in place on a block of samples; the knobs arrive as raw 0..10 values.
namespace fx
{
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float knobDb (float knob) { return (knob - 5.0f) * 2.4f; }   // 5 = 0 dB, +/-12 dB at the ends

    // Runs f(data, count) on the 2x oversampled signal, in sub-blocks no bigger than maxBlock.
    template <typename F>
    void processOversampled (juce::dsp::Oversampling<float>& os, float* x, int n, int maxBlock, F&& f)
    {
        float* chans[1] = { x };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        for (size_t start = 0; start < (size_t) n; start += (size_t) maxBlock)
        {
            auto sub = block.getSubBlock (start, std::min ((size_t) maxBlock, (size_t) n - start));
            auto up = os.processSamplesUp (sub);
            f (up.getChannelPointer (0), up.getNumSamples());
            os.processSamplesDown (sub);
        }
    }

    // Click-free bypass: crossfades between the dry and the processed signal when a pedal is switched.
    class PedalSwitch
    {
    public:
        void prepare (double sampleRate, bool on)
        {
            mix.reset (sampleRate, 0.01);
            mix.setCurrentAndTargetValue (on ? 1.0f : 0.0f);
        }

        void setOn (bool on) { mix.setTargetValue (on ? 1.0f : 0.0f); }
        bool needsProcessing() const { return mix.isSmoothing() || mix.getTargetValue() > 0.5f; }

        void blend (const float* dry, float* wet, int n)
        {
            for (int i = 0; i < n; ++i)
                wet[i] = dry[i] + mix.getNextValue() * (wet[i] - dry[i]);
        }

        void blend (const float* dryL, const float* dryR, float* wetL, float* wetR, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                const float m = mix.getNextValue();
                wetL[i] = dryL[i] + m * (wetL[i] - dryL[i]);
                wetR[i] = dryR[i] + m * (wetR[i] - dryR[i]);
            }
        }

    private:
        juce::SmoothedValue<float> mix;
    };

    // Keeps the pick attack out of the reverb. The input is delayed about a millisecond while a fast and a slow
    // level follower watch it; when the fast one jumps ahead (a pick), the send is dipped for a few milliseconds.
    // The note itself still feeds the reverb, so the tail blooms just after the attack instead of hissing with it.
    class ReverbDuck
    {
    public:
        void prepare (double sampleRate);
        void reset();
        float process (float in);

    private:
        std::vector<float> line;
        size_t pos = 0;
        float fast = 0.0f, slow = 0.0f, gain = 1.0f;
        float fastAtk = 0.0f, fastRel = 0.0f, slowAtk = 0.0f, slowRel = 0.0f, gainAtk = 0.0f, gainRel = 0.0f;
    };

    // Closes on the pickup hum and hiss between notes. Knob 0 = off.
    class NoiseGate
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void process (float* x, int n, float knob);

    private:
        float env = 0.0f, gain = 1.0f, envRelease = 0.0f, openCoef = 0.0f, closeCoef = 0.0f;
        int hold = 0, holdSamples = 0;
        bool open = true;
    };

    // Transposer: a read head that moves through a delay line faster or slower than the write head. When the delay
    // drifts too far the head jumps by about one window, and the landing spot is chosen by cross-correlation so the
    // waveform lines up across the splice (no warble). Adds roughly 25 ms of delay while shifting.
    class PitchShifter
    {
    public:
        void prepare (double sampleRate);
        void process (float* x, int n, float semitones);

    private:
        float read (double pos) const;
        void startSplice (double ratio);

        std::vector<float> buffer, reference;
        int mask = 0, fadeIndex = 0, fadeLength = 0, searchRange = 0, corrLength = 0, jump = 0;
        long long count = 0;
        double headPos = 0.0, oldPos = 0.0, minDelay = 0.0, maxDelay = 0.0;
        juce::SmoothedValue<float> semis, active;
    };

    // Wah: swept band-pass filter. Manual follows the position knob; auto follows how hard you pick.
    class Wah
    {
    public:
        void prepare (double sampleRate);
        void reset();
        void process (float* x, int n, bool autoMode, float posKnob, float sensKnob, float resKnob);

    private:
        double sr = 44100.0;
        float envAttack = 0.0f, envRelease = 0.0f, env = 0.0f, pos = 0.5f, posCoef = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
    };

    class Compressor
    {
    public:
        void prepare (double sampleRate);
        void reset() { gr = 0.0f; }
        void process (float* x, int n, float sustainKnob, float attackKnob, float levelKnob);

    private:
        double sr = 44100.0;
        float gr = 0.0f, releaseCoef = 0.0f;
    };

    // Tube Screamer: the clean signal plus a clipped, mid-boosted copy, so the bass stays tight and the pick stays clear.
    class TubeScreamer
    {
    public:
        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void process (float* x, int n, float driveKnob, float toneKnob, float levelKnob);

    private:
        juce::dsp::Oversampling<float> os { 1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
        double osRate = 88200.0;
        int maxBlock = 512;
        float lowCoef = 0.0f, low = 0.0f, t1 = 0.0f, t2 = 0.0f;
    };

    // Overdrive: asymmetric tube-style clipping of the whole signal. Cranked, this is the heavy one.
    class Overdrive
    {
    public:
        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void process (float* x, int n, float driveKnob, float toneKnob, float levelKnob);

    private:
        juce::dsp::Oversampling<float> os { 1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
        double osRate = 88200.0;
        int maxBlock = 512;
        float hpCoef = 0.0f, dcCoef = 0.0f, hpLow = 0.0f, dc = 0.0f, t1 = 0.0f, t2 = 0.0f;
    };

    // 8-band graphic equalizer on the stereo mix.
    class GraphicEq
    {
    public:
        void prepare (double sampleRate, int maxBlockSize);
        void process (float* l, float* r, int n, const std::array<float, Params::numEqBands>& gainsDb);

    private:
        using Filter = juce::dsp::IIR::Filter<float>;
        std::array<std::array<Filter, Params::numEqBands>, 2> filters;
        std::array<float, Params::numEqBands> last {};
        double sr = 44100.0;
    };

    class Tremolo
    {
    public:
        void prepare (double sampleRate);
        void process (float* l, float* r, int n, bool on, float rateKnob, float depthKnob);

    private:
        double sr = 44100.0;
        float phase = 0.0f;
        juce::SmoothedValue<float> depth;
    };

    class DelayFx
    {
    public:
        void prepare (double sampleRate);
        void process (float* l, float* r, int n, bool on, float timeMs, float fbKnob, float toneKnob, float mixKnob);

    private:
        double sr = 44100.0;
        std::array<std::vector<float>, 2> buf;
        std::array<float, 2> lp {};
        int size = 0, writePos = 0;
        juce::SmoothedValue<float> time, mix;
    };

    struct PostParams
    {
        bool phOn = false, chOn = false, trOn = false, dlOn = false, rvOn = false;
        float phRate = 0, phDepth = 0, phFb = 0, phMix = 0;
        float chRate = 0, chDepth = 0, chMix = 0;
        float trRate = 0, trDepth = 0;
        float dlTime = 380, dlFb = 0, dlTone = 5, dlMix = 0;
        float rvSize = 5, rvDamp = 5, rvMix = 0;
    };

    // Phaser -> chorus -> tremolo -> delay -> digital reverb.
    class PostFx
    {
    public:
        void prepare (double sampleRate, int maxBlockSize);
        void process (float* l, float* r, int n, const PostParams& p);

    private:
        void processChunk (float* l, float* r, int n, const PostParams& p);

        double sr = 44100.0;
        int maxBlock = 512;
        juce::dsp::Phaser<float> phaser;
        juce::dsp::Chorus<float> chorus;
        juce::dsp::Reverb reverb;
        Tremolo tremolo;
        DelayFx delay;
        juce::SmoothedValue<float> reverbMix;
        std::vector<float> wetL, wetR;
    };
}
