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
        slowDepth = (float) (0.0004 * sampleRate);
        buf.assign ((size_t) (baseDelay + slowDepth) + 4, 0.0f);
        fb = feedback;
        dispersion = dispersionCoef;
        lpCoef = (float) (1.0 - std::exp (-twoPi * 2800.0 / sampleRate));
        hpCoef = (float) (1.0 - std::exp (-twoPi * 120.0 / sampleRate));
        slow.prepare (sampleRate, 1.3, seed);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        apState.fill (0.0f);
        writePos = 0;
        lp = lp2 = hp = 0.0f;
    }

    float process (float in)
    {
        const float slowMod = slow.next();
        const float delay = baseDelay + slowDepth * slowMod;

        float readPos = (float) writePos - delay;
        if (readPos < 0.0f)
            readPos += (float) buf.size();
        const int i0 = (int) readPos;
        const int i1 = i0 + 1 >= (int) buf.size() ? 0 : i0 + 1;
        const float frac = readPos - (float) i0;
        float x = buf[(size_t) i0] + frac * (buf[(size_t) i1] - buf[(size_t) i0]);

        for (auto& s : apState)
        {
            const float y = dispersion * x + s;
            s = x - dispersion * y;
            x = y;
        }

        x = std::tanh (x * 1.5f) * (1.0f / 1.5f);   // springs compress when driven hard

        lp += lpCoef * (x - lp);   // damping: each pass loses some highs (two poles = 12 dB/octave)
        lp2 += lpCoef * (lp - lp2);
        x = lp2;
        hp += hpCoef * (x - hp);   // keeps low-end rumble from building up in the loop
        x -= hp;

        buf[writePos] = in + fb * x;
        if (++writePos >= buf.size())
            writePos = 0;
        return x;
    }

private:
    static constexpr int numStages = 30;

    std::vector<float> buf;
    std::array<float, numStages> apState {};
    Wander slow;
    size_t writePos = 0;
    float baseDelay = 1.0f, slowDepth = 0.0f;
    float fb = 0.0f, dispersion = -0.65f, lpCoef = 0.0f, hpCoef = 0.0f, lp = 0.0f, lp2 = 0.0f, hp = 0.0f;
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
        inLpCoef = (float) (1.0 - std::exp (-twoPi * 2200.0 / sampleRate));
        inHpCoef = (float) (1.0 - std::exp (-twoPi * 150.0 / sampleRate));
        reset();
    }

    void reset()
    {
        springA.reset();
        springB.reset();
        springC.reset();
        inLp = inLp2 = inHp = 0.0f;
    }

    void process (float in, float& left, float& right)
    {
        // The pick attack is mostly high frequencies; keep it out of the springs so it doesn't smear into hiss.
        inLp += inLpCoef * (in - inLp);
        inLp2 += inLpCoef * (inLp - inLp2);
        inHp += inHpCoef * (inLp2 - inHp);
        const float x = inLp2 - inHp;

        const float a = springA.process (x);
        const float b = springB.process (x);
        const float c = springC.process (x);
        left  = outScale * (0.8f * a + 0.3f * b + 0.6f * c);
        right = outScale * (0.8f * b + 0.3f * a + 0.6f * c);
    }

private:
    static constexpr float outScale = 1.1f;

    SpringLine springA, springB, springC;
    float inLp = 0.0f, inLp2 = 0.0f, inHp = 0.0f, inLpCoef = 0.0f, inHpCoef = 0.0f;
};
