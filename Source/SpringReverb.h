#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Cheap xorshift noise so the audio thread never touches a heavyweight RNG.
class Noise
{
public:
    explicit Noise (uint32_t seed = 1) : state (seed != 0 ? seed : 1u) {}

    float next()   // -1..1
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }

private:
    uint32_t state;
};

// A slowly wandering value: white noise through a one-pole lowpass, scaled so that
// three standard deviations reach +/-1.
class Wander
{
public:
    void prepare (double sampleRate, double freqHz, uint32_t seed)
    {
        noise = Noise (seed);
        coef = (float) (1.0 - std::exp (-6.283185307179586 * freqHz / sampleRate));
        gain = (float) (1.0 / std::sqrt ((1.0 / 3.0) * coef / (2.0 - coef)));   // uniform noise has variance 1/3
        value = 0.0f;
    }

    float next()
    {
        value += coef * (noise.next() - value);
        return std::clamp (value * gain * (1.0f / 3.0f), -1.0f, 1.0f);
    }

private:
    Noise noise;
    float coef = 0.0f, gain = 1.0f, value = 0.0f;
};

// One spring: a delay line in a feedback loop with a chain of first-order allpasses.
// The allpasses delay low frequencies more than high ones, so each trip round the loop
// smears the signal into the "boing" chirp of a real spring tank.
// Real springs are never perfectly steady, so the delay length drifts randomly (slow wander
// plus a faint fast flutter), the dispersion breathes a little, and the loop compresses
// gently when hit hard.
class SpringLine
{
public:
    void prepare (double sampleRate, float delaySeconds, float feedback, float dispersionCoef, uint32_t seed)
    {
        constexpr double twoPi = 6.283185307179586;
        baseDelay = (float) (delaySeconds * sampleRate);
        slowDepth = (float) (0.0006 * sampleRate);
        fastDepth = (float) (0.0001 * sampleRate);
        buf.assign ((size_t) (baseDelay + slowDepth + fastDepth) + 4, 0.0f);
        fb = feedback;
        dispersion = dispersionCoef;
        lpCoef = (float) (1.0 - std::exp (-twoPi * 3800.0 / sampleRate));
        hpCoef = (float) (1.0 - std::exp (-twoPi * 120.0 / sampleRate));
        slow.prepare (sampleRate, 1.3, seed);
        fast.prepare (sampleRate, 7.0, seed * 2654435761u + 1u);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        apState.fill (0.0f);
        writePos = 0;
        lp = hp = 0.0f;
    }

    float process (float in)
    {
        const float slowMod = slow.next();
        const float delay = baseDelay + slowDepth * slowMod + fastDepth * fast.next();

        float readPos = (float) writePos - delay;
        if (readPos < 0.0f)
            readPos += (float) buf.size();
        const int i0 = (int) readPos;
        const int i1 = i0 + 1 >= (int) buf.size() ? 0 : i0 + 1;
        const float frac = readPos - (float) i0;
        float x = buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);

        const float a = dispersion + 0.02f * slowMod;
        for (auto& s : apState)
        {
            const float y = a * x + s;
            s = x - a * y;
            x = y;
        }

        x = std::tanh (x * 1.5f) * (1.0f / 1.5f);   // springs compress when driven hard

        lp += lpCoef * (x - lp);   // damping: each pass loses some highs
        x = lp;
        hp += hpCoef * (x - hp);   // keeps low-end rumble from building up in the loop
        x -= hp;

        buf[writePos] = in + fb * x;
        if (++writePos >= buf.size())
            writePos = 0;
        return x;
    }

private:
    static constexpr int numStages = 60;

    std::vector<float> buf;
    std::array<float, numStages> apState {};
    Wander slow, fast;
    size_t writePos = 0;
    float baseDelay = 1.0f, slowDepth = 0.0f, fastDepth = 0.0f;
    float fb = 0.0f, dispersion = -0.65f, lpCoef = 0.0f, hpCoef = 0.0f, lp = 0.0f, hp = 0.0f;
};

// Three springs of different length and character, spread left/right, fed from a band-limited mono input.
class SpringReverb
{
public:
    void prepare (double sampleRate)
    {
        constexpr double twoPi = 6.283185307179586;
        springA.prepare (sampleRate, 0.0270f, 0.80f, -0.65f, 0x1234abcdu);
        springB.prepare (sampleRate, 0.0335f, 0.80f, -0.60f, 0x9e3779b9u);
        springC.prepare (sampleRate, 0.0391f, 0.78f, -0.70f, 0x51ed270bu);
        inLpCoef = (float) (1.0 - std::exp (-twoPi * 4500.0 / sampleRate));
        inHpCoef = (float) (1.0 - std::exp (-twoPi * 150.0 / sampleRate));
        reset();
    }

    void reset()
    {
        springA.reset();
        springB.reset();
        springC.reset();
        inLp = inHp = 0.0f;
    }

    void process (float in, float& left, float& right)
    {
        inLp += inLpCoef * (in - inLp);
        inHp += inHpCoef * (inLp - inHp);
        const float x = inLp - inHp;

        const float a = springA.process (x);
        const float b = springB.process (x);
        const float c = springC.process (x);
        left  = outScale * (0.8f * a + 0.3f * b + 0.6f * c);
        right = outScale * (0.8f * b + 0.3f * a + 0.6f * c);
    }

private:
    static constexpr float outScale = 0.75f;

    SpringLine springA, springB, springC;
    float inLp = 0.0f, inHp = 0.0f, inLpCoef = 0.0f, inHpCoef = 0.0f;
};
