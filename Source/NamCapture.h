#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

namespace nam { class DSP; }

// NAM captures packed by tools/pack_nam.py and embedded into the program (personal builds only).
class NamPack
{
public:
    enum Group { dumble = 0, ts9 = 1, sd1 = 2 };

    NamPack (const void* data, size_t size);
    bool isValid() const { return ! entries.empty(); }

    int count (int group) const;
    juce::String name (int group, int index) const;   // captures of a group in knob order
    float order (int group, int index) const;          // what the knob order was made from (gain, or the drive setting)

    // Builds a ready-to-run model (slow: call it off the audio thread). Null if anything is wrong with it.
    std::unique_ptr<nam::DSP> load (int group, int index, double sampleRate, int maxBlockSize) const;
    juce::String getLastError() const { return lastError; }

private:
    struct Entry { int group; float order; juce::String name; std::string json; };
    std::vector<const Entry*> inGroup (int group) const;
    std::vector<Entry> entries;
    mutable juce::String lastError;
};

// Plays one NAM capture on the audio thread. New captures are handed over from another thread and cross-faded in,
// so turning a knob that changes the capture never clicks.
class NamPlayer
{
public:
    NamPlayer();
    ~NamPlayer();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // from the loading thread: the new capture, plus the gains that put the guitar and the result at the right level
    void post (std::unique_ptr<nam::DSP> model, float inputGain, float outputGain);
    void collectGarbage();   // from the loading thread
    bool hasModel() const { return current != nullptr; }
    void update();   // audio thread: picks up a newly loaded capture (process() does this too)

    void process (float* x, int n);   // in place

private:
    struct Slot { std::unique_ptr<nam::DSP> dsp; float inputGain = 1.0f, outputGain = 1.0f; };
    void run (Slot& slot, const float* in, float* out, int n);

    std::atomic<Slot*> inbox { nullptr }, outbox { nullptr };
    Slot* current = nullptr;
    Slot* previous = nullptr;
    float fade = 1.0f, fadeStep = 0.001f;
    int maxBlock = 512;
    std::vector<float> in, a, b;
};

// The input and output gains for a capture, from the levels written into it. Amps are brought to one loudness;
// pedals keep their real output level (a boost should be louder).
void namLevels (nam::DSP& model, float& inputGain, float& outputGain, bool matchLoudness = true);
