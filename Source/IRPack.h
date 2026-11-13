#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// Recorded impulse responses packed by tools/pack_irs.py and embedded into the program (personal builds only):
// three microphone choices per cabinet and the 10 x 10 Dwell / Tone grid of a Fender spring reverb tank.
class IRPack
{
public:
    enum Group { fenderCab = 0, twoRockCab = 1, spring = 2 };

    // parses the zlib-compressed pack; isValid() is false when the data is missing or damaged
    IRPack (const void* data, size_t size);
    bool isValid() const { return ! entries.empty(); }

    // Cabinet for an amp (0 Fender, 1 Two-Rock) with the MIC knob (0..10) blending its three microphones,
    // dark (0) through balanced (5) to bright (10). Level-matched at 1 kHz.
    bool cabinet (int model, float micKnob, juce::AudioBuffer<float>& out, double& rate) const;

    // Spring tank IR for DWELL and TONE positions 1..10
    bool springTank (int dwell, int tone, juce::AudioBuffer<float>& out, double& rate) const;

private:
    struct Entry { juce::uint32 group, a, b, rate, length, format; float scale; juce::uint32 offset; };
    const Entry* find (juce::uint32 group, juce::uint32 a, juce::uint32 b) const;
    void decode (const Entry& e, float* dest, int length) const;

    std::vector<Entry> entries;
    juce::MemoryBlock samples;
};
