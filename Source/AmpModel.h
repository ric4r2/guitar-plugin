#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

// A blackface Fender (AB763 Deluxe Reverb style) amp, built from its circuit:
//   12AX7 stage (V1A) -> treble/middle/bass tone stack -> volume pot -> 12AX7 stage (V1B)
//   -> phase inverter and push-pull power amp with supply sag. The speaker cabinet is in CabinetIR.h.
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
    // Two-Rock style (Dumble lineage) clean channel. Two-Rock does not publish schematics, so these are the same
    // treble/middle/bass network with the values that give its known voice: a 25k middle pot and smaller bass and
    // middle caps, so the mids are much less scooped than a blackface and the low end stays tight.
    inline constexpr ToneStackValues twoRock { 250e3, 250e3, 25e3, 100e3, 250e-12, 47e-9, 22e-9 };

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
        void setSag (float amount) { sag = amount; }   // 0.18 for a small 6V6 amp, less for a big, stiff supply
        float process (float v, float drive);   // v in volts from V1B; returns about -1..1

    private:
        float env = 0.0f, attack = 0.0f, release = 0.0f, sag = 0.18f;
    };
}
