#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
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

// The captures are recorded at 48 kHz (or 44.1) and only sound right at that rate. At 88.2/96 kHz (or 176.4/192) they
// run at half (a quarter) of the host's rate instead, through a 2x linear-phase resampler: that keeps their tone, and
// halves the work. 1, 2 or 4:
int namRateFactor (double hostSampleRate);

// Brings a signal down to rate / factor, lets a function work on it there, and brings it back up. The delay this adds
// is a fixed number of samples at the host rate (latency()).
class RateBridge
{
public:
    void prepare (int factor, int maxBlockSize);
    void reset();
    int getFactor() const { return factor; }
    int latency() const { return latencySamples; }
    int maxLowBlock() const { return maxBlock / juce::jmax (1, factor) + 2; }

    template <typename F>
    void process (float* x, int n, F&& atLowRate)
    {
        int count = n;
        const float* src = x;
        for (size_t s = 0; s < stages.size(); ++s)
        {
            count = stages[s].decimate (src, count, levels[s].data());
            src = levels[s].data();
        }
        atLowRate (levels.back().data(), count);
        for (size_t s = stages.size(); s-- > 0;)
        {
            float* dst = s == 0 ? upOut.data() : levels[s - 1].data();
            count = stages[s].interpolate (levels[s].data(), count, dst);
        }
        const int size = (int) fifo.size();
        for (int i = 0; i < count; ++i)
            fifo[(size_t) ((fifoWrite + i) % size)] = upOut[(size_t) i];
        fifoWrite = (fifoWrite + count) % size;
        for (int i = 0; i < n; ++i)
            x[i] = fifo[(size_t) ((fifoRead + i) % size)];
        fifoRead = (fifoRead + n) % size;
    }

private:
    struct Stage   // halving / doubling with a 47-tap half-band filter
    {
        static constexpr int taps = 47;
        std::array<float, taps> h {};
        std::array<float, taps * 2> downZ {}, upZ {};
        int downPos = 0, upPos = 0, phase = 0;
        void design();
        void clear();
        int decimate (const float* in, int n, float* out);
        int interpolate (const float* in, int n, float* out);
    };
    int factor = 1, maxBlock = 512, latencySamples = 0;
    std::vector<Stage> stages;
    std::vector<std::vector<float>> levels;   // the signal after each halving
    std::vector<float> upOut, fifo;
    int fifoRead = 0, fifoWrite = 0;
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

    // what to build the models for (see namRateFactor), and the delay the player adds at the host rate
    double modelRate() const { return hostRate / bridge.getFactor(); }
    int modelBlockSize() const { return bridge.getFactor() > 1 ? bridge.maxLowBlock() : maxBlock; }
    int latencySamples() const { return bridge.getFactor() > 1 ? bridge.latency() : 0; }

    // from the loading thread: the new capture, plus the gains that put the guitar and the result at the right level
    // delay: how many samples (at the model's rate) the capture takes to come through, from namDelay()
    void post (std::unique_ptr<nam::DSP> model, float inputGain, float outputGain, int delay = 0);
    // audio thread: everything the player delays the signal by right now (the bridge plus the capture), host samples
    int totalDelay() const { return latencySamples() + (current != nullptr ? current->delay * bridge.getFactor() : 0); }
    void collectGarbage();   // from the loading thread
    bool hasModel() const { return current != nullptr; }
    void update();   // audio thread: picks up a newly loaded capture (process() does this too)

    void process (float* x, int n);   // in place

private:
    struct Slot { std::unique_ptr<nam::DSP> dsp; float inputGain = 1.0f, outputGain = 1.0f; int delay = 0; };
    void run (Slot& slot, const float* in, float* out, int n);
    void processNative (float* x, int n);   // at the captures' own rate
    RateBridge bridge;
    double hostRate = 48000.0;

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

// How many samples a capture takes to come through (each recording rig has its own delay: the Dumble captures take
// 0.3 to 0.5 ms). Found with a quiet noise burst, so a tone's repeating peaks can't fool it. Slow: not on the audio
// thread. Leaves the model reset and ready.
int namDelay (nam::DSP& model, double sampleRate, int maxBlockSize, float inputGain);
