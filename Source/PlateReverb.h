#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Smooth, warm reverb: input diffusers feeding an 8-line feedback delay network.
// Unlike a spring there is no dispersion, so a pick attack never smears into a swept "sizzle";
// the highs are filtered before and inside the network so the tail stays dark and quiet.
class PlateReverb
{
public:
    void prepare (double sampleRate)
    {
        constexpr double twoPi = 6.283185307179586;
        static constexpr int baseLengths[numLines] = { 1103, 1297, 1523, 1709, 1931, 2129, 2347, 2543 };
        static constexpr int apLengths[numAllpass] = { 113, 181, 263 };
        const double scale = sampleRate / 48000.0;
        const double rt60 = 1.7;

        for (int i = 0; i < numLines; ++i)
        {
            const int len = std::max (16, (int) std::lround (baseLengths[i] * scale));
            lines[(size_t) i].assign ((size_t) len, 0.0f);
            lineGain[(size_t) i] = (float) std::pow (10.0, -3.0 * len / (rt60 * sampleRate));
        }
        for (int i = 0; i < numAllpass; ++i)
            allpass[(size_t) i].buf.assign ((size_t) std::max (8, (int) std::lround (apLengths[i] * scale)), 0.0f);

        dampCoef = (float) (1.0 - std::exp (-twoPi * 2200.0 / sampleRate));
        inLpCoef = (float) (1.0 - std::exp (-twoPi * 2400.0 / sampleRate));
        inHpCoef = (float) (1.0 - std::exp (-twoPi * 150.0 / sampleRate));
        reset();
    }

    void reset()
    {
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);
        for (auto& a : allpass)
        {
            std::fill (a.buf.begin(), a.buf.end(), 0.0f);
            a.pos = 0;
        }
        pos.fill (0);
        damp.fill (0.0f);
        inLp = inLp2 = inHp = 0.0f;
    }

    void process (float in, float& left, float& right)
    {
        inLp += inLpCoef * (in - inLp);
        inLp2 += inLpCoef * (inLp - inLp2);
        inHp += inHpCoef * (inLp2 - inHp);
        float x = inLp2 - inHp;

        for (auto& a : allpass)
            x = a.process (x);

        std::array<float, numLines> v;
        float sum = 0.0f;
        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            damp[i] += dampCoef * (lines[i][pos[i]] - damp[i]);
            v[i] = damp[i] * lineGain[i];
            sum += v[i];
        }

        constexpr float householder = 2.0f / numLines;   // orthogonal mixing matrix: energy is preserved
        float outL = 0.0f, outR = 0.0f;
        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            lines[i][pos[i]] = inject[i] * x * 0.3f + v[i] - householder * sum;
            if (++pos[i] >= lines[i].size())
                pos[i] = 0;
            outL += patternL[i] * v[i];
            outR += patternR[i] * v[i];
        }

        left = outScale * outL;
        right = outScale * outR;
    }

private:
    static constexpr int numLines = 8;
    static constexpr int numAllpass = 3;
    static constexpr float outScale = 0.85f;

    struct Allpass
    {
        std::vector<float> buf;
        size_t pos = 0;
        static constexpr float g = 0.6f;

        float process (float x)
        {
            const float delayed = buf[pos];
            const float v = x - g * delayed;
            buf[pos] = v;
            if (++pos >= buf.size())
                pos = 0;
            return delayed + g * v;
        }
    };

    std::array<std::vector<float>, numLines> lines;
    std::array<size_t, numLines> pos {};
    std::array<float, numLines> lineGain {}, damp {};
    std::array<Allpass, numAllpass> allpass;
    float dampCoef = 0.0f, inLpCoef = 0.0f, inHpCoef = 0.0f, inLp = 0.0f, inLp2 = 0.0f, inHp = 0.0f;

    static constexpr std::array<float, numLines> inject   { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };
    static constexpr std::array<float, numLines> patternL { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };
    static constexpr std::array<float, numLines> patternR { 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f };
};
