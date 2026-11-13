#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// One spring: a delay line in a feedback loop with a chain of first-order allpasses.
// The allpasses delay low frequencies more than high ones, so each trip round the loop
// smears the signal into the "boing" chirp of a real spring tank.
class SpringLine
{
public:
    void prepare (double sampleRate, float delaySeconds, float feedback)
    {
        constexpr double twoPi = 6.283185307179586;
        buf.assign ((size_t) std::max (1, (int) std::lround (delaySeconds * sampleRate)), 0.0f);
        fb = feedback;
        lpCoef = (float) (1.0 - std::exp (-twoPi * 3800.0 / sampleRate));
        hpCoef = (float) (1.0 - std::exp (-twoPi * 120.0 / sampleRate));
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        apState.fill (0.0f);
        pos = 0;
        lp = hp = 0.0f;
    }

    float process (float in)
    {
        float x = buf[pos];

        for (auto& s : apState)
        {
            const float y = dispersion * x + s;
            s = x - dispersion * y;
            x = y;
        }

        lp += lpCoef * (x - lp);   // damping: each pass loses some highs
        x = lp;
        hp += hpCoef * (x - hp);   // keeps low-end rumble from building up in the loop
        x -= hp;

        buf[pos] = in + fb * x;
        if (++pos >= buf.size())
            pos = 0;
        return x;
    }

private:
    static constexpr int numStages = 60;
    static constexpr float dispersion = -0.65f;

    std::vector<float> buf;
    std::array<float, numStages> apState {};
    size_t pos = 0;
    float fb = 0.0f, lpCoef = 0.0f, hpCoef = 0.0f, lp = 0.0f, hp = 0.0f;
};

// Two springs of different length, panned left/right, fed from a band-limited mono input.
class SpringReverb
{
public:
    void prepare (double sampleRate)
    {
        constexpr double twoPi = 6.283185307179586;
        springA.prepare (sampleRate, 0.0270f, 0.80f);
        springB.prepare (sampleRate, 0.0335f, 0.80f);
        inLpCoef = (float) (1.0 - std::exp (-twoPi * 4500.0 / sampleRate));
        inHpCoef = (float) (1.0 - std::exp (-twoPi * 150.0 / sampleRate));
        reset();
    }

    void reset()
    {
        springA.reset();
        springB.reset();
        inLp = inHp = 0.0f;
    }

    void process (float in, float& left, float& right)
    {
        inLp += inLpCoef * (in - inLp);
        inHp += inHpCoef * (inLp - inHp);
        const float x = inLp - inHp;

        const float a = springA.process (x);
        const float b = springB.process (x);
        left  = outScale * (a + 0.35f * b);
        right = outScale * (b + 0.35f * a);
    }

private:
    static constexpr float outScale = 0.9f;

    SpringLine springA, springB;
    float inLp = 0.0f, inHp = 0.0f, inLpCoef = 0.0f, inHpCoef = 0.0f;
};
