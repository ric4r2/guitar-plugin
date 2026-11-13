#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

struct PitchResult
{
    bool valid = false;
    float frequency = 0.0f;
    float confidence = 0.0f;
};

// YIN pitch detector, tuned for guitar (about 50 Hz to 1.5 kHz). It expects a frame of
// frameSize samples at a reduced sample rate (the processor decimates the input to about 24 kHz).
class PitchDetector
{
public:
    static constexpr int windowSize = 1024;
    static constexpr int maxLag = 640;
    static constexpr int frameSize = windowSize + maxLag;

    PitchDetector() : diff ((size_t) maxLag, 0.0f), cmnd ((size_t) maxLag, 1.0f) {}

    PitchResult detect (const float* x, double sampleRate)
    {
        PitchResult r;

        double energy = 0.0;
        for (int i = 0; i < windowSize; ++i)
            energy += (double) x[i] * x[i];
        if (std::sqrt (energy / windowSize) < 0.002)   // too quiet to trust
            return r;

        for (int tau = 1; tau < maxLag; ++tau)
        {
            double sum = 0.0;
            for (int j = 0; j < windowSize; ++j)
            {
                const double d = (double) x[j] - (double) x[j + tau];
                sum += d * d;
            }
            diff[(size_t) tau] = (float) sum;
        }

        cmnd[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau < maxLag; ++tau)
        {
            running += diff[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
        }

        const int tauMin = std::max (2, (int) (sampleRate / 1500.0));
        const int tauMax = std::min (maxLag - 2, (int) (sampleRate / 50.0));

        int tau = -1;
        for (int t = tauMin; t < tauMax; ++t)
        {
            if (cmnd[(size_t) t] < threshold)
            {
                while (t + 1 < tauMax && cmnd[(size_t) t + 1] < cmnd[(size_t) t])
                    ++t;
                tau = t;
                break;
            }
        }
        if (tau < 0)
            return r;

        float better = (float) tau;   // parabolic interpolation for sub-sample accuracy
        const float a = cmnd[(size_t) tau - 1], b = cmnd[(size_t) tau], c = cmnd[(size_t) tau + 1];
        const float denom = a - 2.0f * b + c;
        if (std::abs (denom) > 1e-9f)
            better += 0.5f * (a - c) / denom;

        r.valid = true;
        r.frequency = (float) (sampleRate / better);
        r.confidence = 1.0f - b;
        return r;
    }

private:
    static constexpr float threshold = 0.15f;
    std::vector<float> diff, cmnd;
};

inline double midiFromFrequency (double hz, double a4 = 440.0)
{
    return 69.0 + 12.0 * std::log2 (hz / a4);
}
