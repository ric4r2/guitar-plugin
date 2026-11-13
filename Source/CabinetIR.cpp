#include "CabinetIR.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>
#include <vector>

namespace cab
{
    namespace
    {
        struct Point { double hz, db; };

        // Close-miked response of each cabinet in dB (0 dB at 1 kHz), smooth part only
        const std::vector<Point>& curve (Model model)
        {
            // open back 1x12: little below 80 Hz, resonance bump, flat mids, the bright alnico-style presence at 2-3 kHz,
            // then a steep fall from about 5 kHz
            static const std::vector<Point> fenderCurve {
                { 20, -48 }, { 40, -26 }, { 60, -13 }, { 80, -4.5 }, { 105, 0.5 }, { 125, 1.8 }, { 160, 0.8 }, { 250, -0.5 },
                { 450, -1.2 }, { 700, -0.4 }, { 1000, 0 }, { 1500, 1.2 }, { 2100, 3.2 }, { 2700, 4.6 }, { 3300, 3.6 },
                { 4200, 0.8 }, { 5000, -3.5 }, { 6000, -10 }, { 7500, -20 }, { 9500, -31 }, { 13000, -44 }, { 24000, -66 }
            };
            // open back 2x12, 65 W Celestion type: deeper and fuller low end, a touch more low-mid body, a smooth,
            // rounded presence and an earlier, softer top-end roll-off
            static const std::vector<Point> twoRockCurve {
                { 20, -42 }, { 40, -20 }, { 60, -8 }, { 80, -1.5 }, { 100, 1.5 }, { 120, 2.6 }, { 160, 2.0 }, { 250, 1.0 },
                { 400, 0.4 }, { 700, 0.2 }, { 1000, 0 }, { 1500, 0.8 }, { 2000, 2.0 }, { 2600, 2.6 }, { 3200, 1.8 },
                { 4000, -0.8 }, { 4800, -4.5 }, { 5800, -11 }, { 7200, -21 }, { 9000, -32 }, { 12500, -46 }, { 24000, -68 }
            };
            return model == twoRock ? twoRockCurve : fenderCurve;
        }

        double curveDb (const std::vector<Point>& c, double hz)
        {
            if (hz <= c.front().hz) return c.front().db;
            if (hz >= c.back().hz)  return c.back().db - 12.0 * std::log2 (hz / c.back().hz);
            for (size_t i = 1; i < c.size(); ++i)
                if (hz < c[i].hz)
                {
                    const double t = std::log (hz / c[i - 1].hz) / std::log (c[i].hz / c[i - 1].hz);
                    const double s = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * t);   // smooth between points
                    return c[i - 1].db + s * (c[i].db - c[i - 1].db);
                }
            return c.back().db;
        }

        // cone break-up: narrow peaks and dips above about 1 kHz, the same every time (fixed seed per cabinet)
        double breakupDb (Model model, double hz)
        {
            static constexpr int count = 14;
            struct Mode { double hz, db, q; };
            static const auto make = [] (unsigned seed, double lowHz, double highHz, double depth)
            {
                std::array<Mode, count> modes {};
                juce::Random rng ((juce::int64) seed);
                for (int i = 0; i < count; ++i)
                {
                    const double f = lowHz * std::pow (highHz / lowHz, (i + rng.nextDouble() * 0.8) / count);
                    const double sign = (i % 2 == 0) ? 1.0 : -1.0;
                    modes[(size_t) i] = { f, sign * depth * (0.5 + rng.nextDouble()) * std::sqrt (f / 1000.0), 6.0 + 10.0 * rng.nextDouble() };
                }
                return modes;
            };
            static const auto fenderModes = make (1957, 900.0, 7000.0, 1.6);
            static const auto twoRockModes = make (1999, 800.0, 6000.0, 1.1);
            const auto& modes = model == twoRock ? twoRockModes : fenderModes;

            double db = 0.0;
            for (const auto& m : modes)
            {
                const double x = m.q * (hz / m.hz - m.hz / hz);
                db += m.db / (1.0 + x * x);
            }
            return db;
        }

        // the inverse transform of juce::dsp::FFT may or may not divide by N; find out once
        double inverseScale (juce::dsp::FFT& fft, int n)
        {
            std::vector<std::complex<float>> a ((size_t) n, 1.0f), b ((size_t) n);
            fft.perform (a.data(), b.data(), true);
            return 1.0 / (double) b[0].real();
        }
    }

    juce::AudioBuffer<float> build (Model model, double sampleRate)
    {
        constexpr int order = 14, n = 1 << order;
        juce::dsp::FFT fft (order);
        const double invScale = inverseScale (fft, n);
        const auto& c = curve (model);

        // log magnitude on the FFT grid
        std::vector<std::complex<float>> spec ((size_t) n), cep ((size_t) n);
        for (int k = 0; k <= n / 2; ++k)
        {
            const double hz = std::max (1.0, (double) k * sampleRate / n);
            const double db = curveDb (c, hz) + (hz > 600.0 ? breakupDb (model, hz) : 0.0);
            const double lnMag = db * std::log (10.0) / 20.0;
            spec[(size_t) k] = (float) lnMag;
            if (k > 0 && k < n / 2)
                spec[(size_t) (n - k)] = (float) lnMag;
        }

        // minimum phase through the real cepstrum: fold the cepstrum onto positive time, then exponentiate
        fft.perform (spec.data(), cep.data(), true);
        for (auto& v : cep)
            v *= (float) invScale;
        for (int i = 1; i < n / 2; ++i)
            cep[(size_t) i] *= 2.0f;
        for (int i = n / 2 + 1; i < n; ++i)
            cep[(size_t) i] = 0.0f;

        fft.perform (cep.data(), spec.data(), false);
        for (auto& v : spec)
            v = std::exp (v);
        fft.perform (spec.data(), cep.data(), true);

        // keep about 46 ms (real cabinet IRs are this short), fading out the end
        const int length = juce::jmin (n, (int) std::lround (0.046 * sampleRate));
        juce::AudioBuffer<float> ir (1, length);
        const int fade = length / 4;
        for (int i = 0; i < length; ++i)
        {
            float w = 1.0f;
            if (i > length - fade)
                w = 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (i - (length - fade)) / (float) fade);
            ir.setSample (0, i, (float) (cep[(size_t) i].real() * invScale) * w);
        }

        normaliseAt1k (ir, sampleRate);
        return ir;
    }

    double magnitudeAt (const juce::AudioBuffer<float>& ir, double sampleRate, double hz)
    {
        std::complex<double> sum = 0.0;
        for (int i = 0; i < ir.getNumSamples(); ++i)
            sum += (double) ir.getSample (0, i) * std::polar (1.0, -juce::MathConstants<double>::twoPi * hz * i / sampleRate);
        return std::abs (sum);
    }

    void normaliseAt1k (juce::AudioBuffer<float>& ir, double sampleRate)
    {
        // average over 800 Hz - 1.25 kHz so one narrow dip in a recorded IR can't throw the level off
        double sum = 0.0;
        int count = 0;
        for (double hz = 800.0; hz <= 1250.0; hz *= 1.02, ++count)
            sum += magnitudeAt (ir, sampleRate, hz);
        const double mag = sum / juce::jmax (1, count);
        if (mag > 1.0e-9)
            ir.applyGain ((float) (1.0 / mag));
    }
}
