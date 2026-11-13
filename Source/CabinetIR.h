#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

// Built-in speaker cabinets as impulse responses, made from the speakers' response curves, so the program works
// without any downloaded files:
//   Fender: Deluxe Reverb style open-back 1x12, Jensen/Oxford-type alnico-voiced 12", close dynamic mic slightly off the cap
//   Two-Rock: open-back 2x12 with 65 W Celestion-type speakers (Two-Rock's TR-65 family): fuller lows, smoother top
// The curve is turned into a minimum-phase impulse response, and cone break-up adds the fine peaks and dips a real
// microphone picks up above about 1 kHz.
namespace cab
{
    enum Model { fender = 0, twoRock = 1 };

    juce::AudioBuffer<float> build (Model model, double sampleRate);

    // scale an impulse response so a 1 kHz tone passes at unity gain (makes cabinets swap at the same loudness)
    void normaliseAt1k (juce::AudioBuffer<float>& ir, double sampleRate);

    // gain of an impulse response at one frequency
    double magnitudeAt (const juce::AudioBuffer<float>& ir, double sampleRate, double hz);
}
