#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

// Click track for any time signature typed in as beats per bar over a note value (2, 4, 8 or 16).
// One pulse is one note of that value, and the BPM is the number of pulses per minute.
class Metronome
{
public:
    static constexpr int minBeats = 1, maxBeats = 16, numNoteValues = 4;

    static int clampBeats (int beats) { return std::clamp (beats, minBeats, maxBeats); }
    static int clampNoteIndex (int i) { return std::clamp (i, 0, numNoteValues - 1); }

    static int noteValue (int noteIndex)
    {
        static const int values[numNoteValues] = { 2, 4, 8, 16 };
        return values[clampNoteIndex (noteIndex)];
    }

    // Index of the allowed note value closest to whatever number was typed in.
    static int noteIndexFor (int typedValue)
    {
        int best = 0;
        for (int i = 1; i < numNoteValues; ++i)
            if (std::abs (noteValue (i) - typedValue) < std::abs (noteValue (best) - typedValue))
                best = i;
        return best;
    }

    // Pulses grouped the way the bar is felt: strong accent on the first pulse, medium on the start of each later group.
    static std::vector<int> groups (int beats, int note)
    {
        beats = clampBeats (beats);
        if (note >= 8 && beats % 3 == 0 && beats > 3)   // 6/8, 9/8, 12/8, 6/16 ...: groups of three
            return std::vector<int> ((size_t) (beats / 3), 3);
        if (beats == 5)
            return { 3, 2 };
        if (beats == 7)
            return { 2, 2, 3 };
        return { beats };
    }

    static int accentFor (int beats, int note, int pulse)   // 2 = strong, 1 = medium, 0 = normal
    {
        int start = 0;
        for (int g : groups (beats, note))
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
    void process (float* l, float* r, int n, bool on, float bpm, int beatsIn, int noteIndex, float volKnob)
    {
        if (! on)
        {
            if (running)
                reset();
            return;
        }

        const int beats = clampBeats (beatsIn);
        const int note = noteValue (noteIndex);
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
                if (pulse >= beats)
                    pulse = 0;

                const int accent = accentFor (beats, note, pulse);
                const float freq = accent == 2 ? 1900.0f : accent == 1 ? 1400.0f : 1000.0f;
                clickInc = 6.2831853f * freq / (float) sr;
                clickEnv = accent == 2 ? 1.0f : accent == 1 ? 0.8f : 0.6f;
                clickPhase = 0.0f;
                shown.store (pulse);
                pulse = (pulse + 1) % beats;
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
