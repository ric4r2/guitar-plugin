#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// A blackface Fender (AB763 Deluxe Reverb style) amp, built from its circuit:
//   12AX7 stage (V1A) -> treble/middle/bass tone stack -> volume pot -> 12AX7 stage (V1B)
//   -> phase inverter and 6V6 push-pull power amp with supply sag -> output transformer -> speaker
// Voltages are real volts: 1.0 in the digital signal is 1 V at the input jack.
namespace amp
{
    // Norman Koren's triode model with his published 12AX7 values (MU 100, EX 1.4, KG1 1060, KP 600, KVB 300).
    // Returns plate current in amps for a grid-cathode and plate-cathode voltage.
    double korenPlateCurrent (double vgk, double vpk);

    // A common-cathode 12AX7 stage with the cathode fully bypassed, as a table from grid signal (volts) to plate
    // signal (volts, inverted like the real stage). Each point is found by solving the stage's load line.
    class TriodeStage
    {
    public:
        void build (double bPlus, double plateLoad, double cathodeResistor, double inputRange);
        float process (float vin) const;

        double getRestPlateVoltage() const { return restPlate; }
        double getSmallSignalGain() const { return gain; }   // |dVout / dVin| at rest

    private:
        std::vector<float> table;
        float range = 1.0f, scale = 0.0f;
        double restPlate = 0.0, gain = 0.0;
    };

    // Fender treble/middle/bass tone stack. The analog transfer function is from Yeh and Smith,
    // "Discretization of the '59 Fender Bassman Tone Stack" (DAFx-06); it is discretised with the bilinear transform.
    struct ToneStackValues { double r1, r2, r3, r4, c1, c2, c3; };

    // AB763 Deluxe Reverb: treble pot 250k, bass pot 250k, slope 100k, 250pF, .1uF, .047uF. The Deluxe has a fixed
    // 6.8k mid resistor; here it is a 13.6k pot so that MID at 5 is exactly the stock amp.
    inline constexpr ToneStackValues deluxeReverb { 250e3, 250e3, 13.6e3, 100e3, 250e-12, 100e-9, 47e-9 };
    // AB763 Twin Reverb: same network with a real 10k middle pot and the smaller treble cap (Guitarix "twin" values)
    inline constexpr ToneStackValues twinReverb { 250e3, 250e3, 10e3, 100e3, 120e-12, 100e-9, 47e-9 };

    class ToneStack
    {
    public:
        void prepare (double sampleRate, const ToneStackValues& values = deluxeReverb);
        void setControls (float treble01, float mid01, float bass01);   // knob positions 0..1
        float process (float x);
        void reset();

        // magnitude of the analog circuit at the current settings (for tests)
        double analogMagnitude (double hz) const;
        // magnitude of the digital filter that is actually running
        double digitalMagnitude (double hz) const;

    private:
        void analogCoefficients (double t, double m, double l, std::array<double, 4>& b, std::array<double, 4>& a) const;

        ToneStackValues v {};
        double fs = 44100.0;
        float lastT = -1.0f, lastM = -1.0f, lastL = -1.0f;
        std::array<double, 4> bz {}, az {}, sb {}, sa {};   // digital and analog coefficients (s^0..s^3)
        double s1 = 0.0, s2 = 0.0, s3 = 0.0;
    };

    // Phase inverter and 6V6 push-pull pair with negative feedback: a symmetric, fairly firm clip, with the supply
    // sagging (less headroom) for a moment after loud passages.
    class PowerAmp
    {
    public:
        void prepare (double sampleRate);
        void reset() { env = 0.0f; }
        float process (float v, float drive);   // v in volts from V1B; returns about -1..1

    private:
        float env = 0.0f, attack = 0.0f, release = 0.0f;
    };

    // Speaker in an open-back 1x12 combo, approximated with filters: little below about 80 Hz, a low resonance,
    // a presence lift around 2.5 kHz and a steep fall above about 5 kHz. Loading a cabinet impulse response is the
    // accurate alternative.
    class Speaker
    {
    public:
        void prepare (double sampleRate, bool twin = false);   // twin: 2x12 open back with brighter, tighter speakers
        void reset();
        void process (float* x, int n);

    private:
        struct Biquad
        {
            double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            float process (float in)
            {
                const double y = b0 * in + z1;
                z1 = b1 * in - a1 * y + z2;
                z2 = b2 * in - a2 * y;
                return (float) y;
            }
        };
        std::array<Biquad, 6> stages;
    };
}
