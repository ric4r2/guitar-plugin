#pragma once
#include <atomic>
#include <cmath>
#include <vector>

// Click track. One pulse is one note of the time signature's denominator (an eighth note in 6/8),
// and the BPM is the number of pulses per minute.
class Metronome
{
public:
    static constexpr int numSigs = 8;

    static int clampSig (int i) { return i < 0 ? 0 : (i >= numSigs ? numSigs - 1 : i); }

    static const char* sigName (int i)
    {
        static const char* names[numSigs] = { "2/4", "3/4", "4/4", "5/4", "6/8", "7/8", "9/8", "12/8" };
        return names[clampSig (i)];
    }

    // Pulses grouped the way the bar is felt: strong accent on the first pulse, medium on the start of each later group.
    static const std::vector<int>& groups (int i)
    {
        static const std::vector<std::vector<int>> g = { { 2 }, { 3 }, { 4 }, { 3, 2 }, { 3, 3 }, { 2, 2, 3 }, { 3, 3, 3 }, { 3, 3, 3, 3 } };
        return g[(size_t) clampSig (i)];
    }

    static int numPulses (int sig)
    {
        int total = 0;
        for (int v : groups (sig))
            total += v;
        return total;
    }

    static int accentFor (int sig, int pulse)   // 2 = strong, 1 = medium, 0 = normal
    {
        int start = 0;
        for (int g : groups (sig))
        {
            if (pulse == start)
                return start == 0 ? 2 : 1;
            start += g;
        }
        return 0;
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        clickDecay = (float) std::exp (-1.0 / (0.012 * sr));
        reset();
    }

    void reset()
    {
        running = false;
        pulse = 0;
        samplesToNext = 0.0;
        clickEnv = 0.0f;
        clickPhase = 0.0f;
        shown.store (-1);
    }

    // Adds the clicks to l and r.
    void process (float* l, float* r, int n, bool on, float bpm, int sig, float volKnob)
    {
        if (! on)
        {
            if (running)
                reset();
            return;
        }

        sig = clampSig (sig);
        const int pulses = numPulses (sig);
        if (! running)
        {
            running = true;
            samplesToNext = 0.0;
            pulse = 0;
        }

        const double pulseLength = sr * 60.0 / std::max (20.0f, bpm);
        const float v = volKnob / 10.0f;
        const float gain = v * v * 0.6f;

        for (int i = 0; i < n; ++i)
        {
            if (samplesToNext <= 0.0)
            {
                if (pulse >= pulses)
                    pulse = 0;

                const int accent = accentFor (sig, pulse);
                const float freq = accent == 2 ? 1900.0f : accent == 1 ? 1400.0f : 1000.0f;
                clickInc = 6.2831853f * freq / (float) sr;
                clickEnv = accent == 2 ? 1.0f : accent == 1 ? 0.8f : 0.6f;
                clickPhase = 0.0f;
                shown.store (pulse);
                pulse = (pulse + 1) % pulses;
                samplesToNext += pulseLength;
            }
            samplesToNext -= 1.0;

            if (clickEnv > 1.0e-4f)
            {
                const float s = std::sin (clickPhase) * clickEnv * gain;
                clickPhase += clickInc;
                clickEnv *= clickDecay;
                l[i] += s;
                r[i] += s;
            }
        }
    }

    int getPulse() const { return shown.load(); }   // -1 when stopped

private:
    double sr = 44100.0;
    bool running = false;
    int pulse = 0;
    double samplesToNext = 0.0;
    float clickEnv = 0.0f, clickPhase = 0.0f, clickInc = 0.0f, clickDecay = 0.99f;
    std::atomic<int> shown { -1 };
};
