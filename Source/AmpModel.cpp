#include "AmpModel.h"
#include <cmath>
#include <complex>

namespace amp
{
    namespace
    {
        double softplus (double x) { return x > 30.0 ? x : std::log1p (std::exp (x)); }

        // RBJ cookbook biquads, used by the speaker
        void setPeak (double fs, double f, double q, double db, double& b0, double& b1, double& b2, double& a1, double& a2)
        {
            const double A = std::pow (10.0, db / 40.0), w = 2.0 * juce::MathConstants<double>::pi * f / fs;
            const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + alpha / A;
            b0 = (1.0 + alpha * A) / a0; b1 = -2.0 * c / a0; b2 = (1.0 - alpha * A) / a0;
            a1 = -2.0 * c / a0; a2 = (1.0 - alpha / A) / a0;
        }

        void setHighPass (double fs, double f, double q, double& b0, double& b1, double& b2, double& a1, double& a2)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + alpha;
            b0 = (1.0 + c) / 2.0 / a0; b1 = -(1.0 + c) / a0; b2 = b0; a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
        }

        void setLowPass (double fs, double f, double q, double& b0, double& b1, double& b2, double& a1, double& a2)
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f / fs, alpha = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + alpha;
            b0 = (1.0 - c) / 2.0 / a0; b1 = (1.0 - c) / a0; b2 = b0; a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
        }
    }

    // ------------------------------------------------------------ 12AX7

    double korenPlateCurrent (double vgk, double vpk)
    {
        constexpr double mu = 100.0, ex = 1.4, kg1 = 1060.0, kp = 600.0, kvb = 300.0;
        if (vpk <= 0.0)
            return 0.0;
        const double e1 = vpk / kp * softplus (kp * (1.0 / mu + vgk / std::sqrt (kvb + vpk * vpk)));
        return e1 > 0.0 ? 2.0 * std::pow (e1, ex) / kg1 : 0.0;
    }

    void TriodeStage::build (double bPlus, double plateLoad, double cathodeResistor, double inputRange)
    {
        // resting point: the cathode resistor sets the bias, the plate load sets the plate voltage
        double lo = 0.0, hi = bPlus / plateLoad;
        for (int i = 0; i < 80; ++i)
        {
            const double ip = 0.5 * (lo + hi), vk = ip * cathodeResistor, vp = bPlus - ip * plateLoad;
            (korenPlateCurrent (-vk, vp - vk) > ip ? lo : hi) = ip;
        }
        const double ipRest = 0.5 * (lo + hi);
        const double vkRest = ipRest * cathodeResistor;   // held there by the bypass capacitor
        restPlate = bPlus - ipRest * plateLoad;

        constexpr int points = 4097;
        table.assign ((size_t) points, 0.0f);
        range = (float) inputRange;
        scale = (float) (points - 1) / (2.0f * range);

        for (int i = 0; i < points; ++i)
        {
            const double vin = -inputRange + 2.0 * inputRange * i / (points - 1);
            double vgk = vin - vkRest;
            if (vgk > 0.0)
                vgk = 0.6 * std::tanh (vgk / 0.6);   // grid current: the grid can't swing much above the cathode

            double plo = vkRest, phi = bPlus;   // plate voltage on the load line
            for (int k = 0; k < 80; ++k)
            {
                const double vp = 0.5 * (plo + phi);
                (vp - (bPlus - plateLoad * korenPlateCurrent (vgk, vp - vkRest)) > 0.0 ? phi : plo) = vp;
            }
            table[(size_t) i] = (float) (0.5 * (plo + phi) - restPlate);
        }

        const int mid = points / 2;
        gain = std::abs ((double) (table[(size_t) mid + 1] - table[(size_t) mid - 1])) / (2.0 * (2.0 * inputRange / (points - 1)));
    }

    float TriodeStage::process (float vin) const
    {
        const float pos = juce::jlimit (0.0f, (float) (table.size() - 1) - 0.001f, (vin + range) * scale);
        const int i = (int) pos;
        const float f = pos - (float) i;
        return table[(size_t) i] + f * (table[(size_t) i + 1] - table[(size_t) i]);
    }

    // ------------------------------------------------------------ tone stack

    void ToneStack::prepare (double sampleRate, const ToneStackValues& values)
    {
        fs = sampleRate;
        v = values;
        lastT = lastM = lastL = -1.0f;
        setControls (0.5f, 0.5f, 0.5f);
        reset();
    }

    void ToneStack::reset()
    {
        s1 = s2 = s3 = 0.0;
    }

    void ToneStack::analogCoefficients (double t, double m, double l, std::array<double, 4>& b, std::array<double, 4>& a) const
    {
        const double R1 = v.r1, R2 = v.r2, R3 = v.r3, R4 = v.r4, C1 = v.c1, C2 = v.c2, C3 = v.c3;

        b[0] = 0.0;
        b[1] = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        b[2] = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
             + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
             + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        b[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
             - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
             + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
             + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
             + t * l * C1 * C2 * C3 * R1 * R2 * R4;

        a[0] = 1.0;
        a[1] = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
        a[2] = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
             - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
             + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
             + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        a[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
             - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
             + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
             + l * C1 * C2 * C3 * R1 * R2 * R4
             + C1 * C2 * C3 * R1 * R3 * R4;
    }

    void ToneStack::setControls (float treble01, float mid01, float bass01)
    {
        if (treble01 == lastT && mid01 == lastM && bass01 == lastL)
            return;
        lastT = treble01; lastM = mid01; lastL = bass01;

        const double t = juce::jlimit (0.0, 1.0, (double) treble01);
        const double m = juce::jlimit (0.0, 1.0, (double) mid01);
        const double l = std::exp (3.4 * (juce::jlimit (0.0, 1.0, (double) bass01) - 1.0));   // audio-taper bass pot

        analogCoefficients (t, m, l, sb, sa);

        // bilinear transform: s = c (1 - z^-1) / (1 + z^-1). Each power of s maps to a fixed polynomial in z^-1.
        const double c = 2.0 * fs;
        static constexpr double poly[4][4] = { { 1, 3, 3, 1 }, { 1, 1, -1, -1 }, { 1, -1, -1, 1 }, { 1, -3, 3, -1 } };
        std::array<double, 4> bn {}, an {};
        double ck = 1.0;
        for (int k = 0; k < 4; ++k)
        {
            for (int j = 0; j < 4; ++j)
            {
                bn[(size_t) j] += sb[(size_t) k] * ck * poly[k][j];
                an[(size_t) j] += sa[(size_t) k] * ck * poly[k][j];
            }
            ck *= c;
        }
        for (int j = 0; j < 4; ++j)
        {
            bz[(size_t) j] = bn[(size_t) j] / an[0];
            az[(size_t) j] = an[(size_t) j] / an[0];
        }
    }

    float ToneStack::process (float x)
    {
        const double y = bz[0] * x + s1;
        s1 = bz[1] * x - az[1] * y + s2;
        s2 = bz[2] * x - az[2] * y + s3;
        s3 = bz[3] * x - az[3] * y;
        return (float) y;
    }

    double ToneStack::analogMagnitude (double hz) const
    {
        const std::complex<double> s (0.0, 2.0 * juce::MathConstants<double>::pi * hz);
        std::complex<double> num = 0.0, den = 0.0, p = 1.0;
        for (int k = 0; k < 4; ++k)
        {
            num += sb[(size_t) k] * p;
            den += sa[(size_t) k] * p;
            p *= s;
        }
        return std::abs (num / den);
    }

    double ToneStack::digitalMagnitude (double hz) const
    {
        const std::complex<double> z1 = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * hz / fs);
        std::complex<double> num = 0.0, den = 0.0, p = 1.0;
        for (int k = 0; k < 4; ++k)
        {
            num += bz[(size_t) k] * p;
            den += az[(size_t) k] * p;
            p *= z1;
        }
        return std::abs (num / den);
    }

    // ------------------------------------------------------------ power amp

    void PowerAmp::prepare (double sampleRate)
    {
        attack = (float) (1.0 - std::exp (-1.0 / (0.005 * sampleRate)));
        release = (float) (1.0 - std::exp (-1.0 / (0.150 * sampleRate)));
        env = 0.0f;
    }

    float PowerAmp::process (float v, float drive)
    {
        const float headroom = 1.0f - 0.18f * juce::jmin (1.0f, env);   // supply sag after loud playing
        const float u = v * drive / headroom;
        // push-pull with negative feedback: clean and symmetric until close to full power, then a firm knee
        const float a = std::abs (u);
        const float clipped = u / std::cbrt (1.0f + a * a * a);
        const float out = headroom * clipped;

        const float level = std::abs (out);
        env += (level > env ? attack : release) * (level - env);
        return out;
    }

    // ------------------------------------------------------------ speaker

    void Speaker::prepare (double sampleRate, bool twin)
    {
        auto& s = stages;
        // 1x12 Jensen-style in the Deluxe; 2x12 in the Twin: tighter low end, more top
        setHighPass (sampleRate, twin ? 85.0 : 75.0, 0.9, s[0].b0, s[0].b1, s[0].b2, s[0].a1, s[0].a2);        // open back: little deep bass
        setPeak (sampleRate, twin ? 100.0 : 110.0, 1.2, 2.5, s[1].b0, s[1].b1, s[1].b2, s[1].a1, s[1].a2);      // cone resonance
        setPeak (sampleRate, twin ? 3000.0 : 2500.0, 1.2, 2.5, s[2].b0, s[2].b1, s[2].b2, s[2].a1, s[2].a2);    // presence
        setLowPass (sampleRate, twin ? 6000.0 : 5200.0, 0.7, s[3].b0, s[3].b1, s[3].b2, s[3].a1, s[3].a2);      // steep top-end fall
        setLowPass (sampleRate, twin ? 7500.0 : 6500.0, 1.0, s[4].b0, s[4].b1, s[4].b2, s[4].a1, s[4].a2);
        setHighPass (sampleRate, 40.0, 0.7, s[5].b0, s[5].b1, s[5].b2, s[5].a1, s[5].a2);           // output transformer low end
        reset();
    }

    void Speaker::reset()
    {
        for (auto& s : stages)
            s.z1 = s.z2 = 0.0;
    }

    void Speaker::process (float* x, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            float y = x[i];
            for (auto& s : stages)
                y = s.process (y);
            x[i] = y;
        }
    }
}
