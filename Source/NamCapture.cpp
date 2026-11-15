#include "NamCapture.h"
#include <juce_core/juce_core.h>
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

// ---------------------------------------------------------------- pack

NamPack::NamPack (const void* data, size_t size)
{
    if (data == nullptr || size == 0)
        return;

    juce::MemoryInputStream compressed (data, size, false);
    juce::GZIPDecompressorInputStream unzip (&compressed, false, juce::GZIPDecompressorInputStream::zlibFormat);
    juce::MemoryBlock all;
    unzip.readIntoMemoryBlock (all);

    juce::MemoryInputStream s (all, false);
    char magic[4] {};
    if (s.read (magic, 4) != 4 || std::memcmp (magic, "JGNM", 4) != 0 || s.readInt() != 1)
        return;
    const int n = s.readInt();
    if (n <= 0 || n > 10000)
        return;

    std::vector<Entry> parsed;
    for (int i = 0; i < n; ++i)
    {
        Entry e;
        e.group = s.readInt();
        e.order = s.readFloat();
        const int nameLength = s.readInt();
        if (nameLength < 0 || nameLength > 1000 || s.getNumBytesRemaining() < nameLength)
            return;
        juce::MemoryBlock nameBytes;
        s.readIntoMemoryBlock (nameBytes, nameLength);
        e.name = nameBytes.toString();
        const int jsonLength = s.readInt();
        if (jsonLength <= 0 || s.getNumBytesRemaining() < jsonLength)
            return;
        e.json.resize ((size_t) jsonLength);
        s.read (e.json.data(), jsonLength);
        parsed.push_back (std::move (e));
    }
    std::stable_sort (parsed.begin(), parsed.end(), [] (const Entry& x, const Entry& y)
                      { return x.group != y.group ? x.group < y.group : x.order < y.order; });
    entries = std::move (parsed);
}

std::vector<const NamPack::Entry*> NamPack::inGroup (int group) const
{
    std::vector<const Entry*> out;
    for (const auto& e : entries)
        if (e.group == group)
            out.push_back (&e);
    return out;
}

int NamPack::count (int group) const   { return (int) inGroup (group).size(); }

juce::String NamPack::name (int group, int index) const
{
    const auto g = inGroup (group);
    return juce::isPositiveAndBelow (index, (int) g.size()) ? g[(size_t) index]->name : juce::String();
}

float NamPack::order (int group, int index) const
{
    const auto g = inGroup (group);
    return juce::isPositiveAndBelow (index, (int) g.size()) ? g[(size_t) index]->order : 0.0f;
}

std::unique_ptr<nam::DSP> NamPack::load (int group, int index, double sampleRate, int maxBlockSize) const
{
    const auto g = inGroup (group);
    if (! juce::isPositiveAndBelow (index, (int) g.size()))
        return nullptr;
    try
    {
        auto model = nam::get_dsp (nlohmann::json::parse (g[(size_t) index]->json));
        if (model == nullptr)
            lastError = "the engine returned no model";
        else
            model->Reset (sampleRate, maxBlockSize);   // also warms it up, so it starts without a thump
        return model;
    }
    catch (const std::exception& e)
    {
        lastError = e.what();
        DBG ("NAM capture failed to load: " << lastError);
        return nullptr;
    }
}

void namLevels (nam::DSP& model, float& inputGain, float& outputGain, bool matchLoudness)
{
    // The plugin works in volts: 1.0 is 1 V peak at the input jack (about -0.8 dBu as a sine). A capture records which
    // analogue level its digital full scale stood for, so the guitar reaches it at the level it was trained at.
    inputGain = model.HasInputLevel() ? (float) juce::Decibels::decibelsToGain (-0.8 - model.GetInputLevel()) : 0.25f;
    if (! matchLoudness)
    {
        // back to volts, using the analogue level the capture's output stood for
        outputGain = model.HasOutputLevel() ? (float) juce::Decibels::decibelsToGain (model.GetOutputLevel() + 0.8) : 1.0f / inputGain;
        return;
    }
    // Captures come out at very different levels: bring each to the same loudness.
    outputGain = model.HasLoudness() ? (float) juce::Decibels::decibelsToGain (-14.0 - model.GetLoudness()) : 1.0f;
}

// ---------------------------------------------------------------- player

NamPlayer::NamPlayer() = default;

NamPlayer::~NamPlayer()
{
    delete inbox.exchange (nullptr);
    delete outbox.exchange (nullptr);
    delete current;
    delete previous;
}

void NamPlayer::prepare (double sampleRate, int maxBlockSize)
{
    maxBlock = juce::jmax (1, maxBlockSize);
    in.assign ((size_t) maxBlock, 0.0f);
    a.assign ((size_t) maxBlock, 0.0f);
    b.assign ((size_t) maxBlock, 0.0f);
    fadeStep = (float) (1.0 / (0.05 * sampleRate));   // 50 ms cross-fade
    // models built for the old settings are useless now; the loading thread sends new ones after prepare()
    delete current;
    delete previous;
    current = previous = nullptr;
    fade = 1.0f;
}

void NamPlayer::reset()
{
    fade = 1.0f;
    if (previous != nullptr && outbox.load() == nullptr)
    {
        outbox.store (previous);
        previous = nullptr;
    }
}

void NamPlayer::post (std::unique_ptr<nam::DSP> model, float inputGain, float outputGain)
{
    auto* slot = new Slot { std::move (model), inputGain, outputGain };
    delete inbox.exchange (slot);   // one the audio thread never picked up
}

void NamPlayer::collectGarbage()
{
    delete outbox.exchange (nullptr);
}

void NamPlayer::run (Slot& slot, const float* src, float* out, int n)
{
    for (int i = 0; i < n; ++i)
        in[(size_t) i] = src[i] * slot.inputGain;
    float* ins[1] = { in.data() };
    float* outs[1] = { out };
    slot.dsp->process (ins, outs, n);
    for (int i = 0; i < n; ++i)
        out[i] *= slot.outputGain;
}

void NamPlayer::update()
{
    // a new capture: start the cross-fade (only one at a time, the old one has to be gone first)
    if (previous == nullptr)
        if (auto* next = inbox.exchange (nullptr))
        {
            previous = current;
            current = next;
            fade = previous != nullptr ? 0.0f : 1.0f;
        }
}

void NamPlayer::process (float* x, int n)
{
    update();

    if (current == nullptr)
        return;   // nothing loaded yet: the signal passes untouched

    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = juce::jmin (maxBlock, n - start);
        float* chunk = x + start;

        run (*current, chunk, a.data(), len);
        if (previous != nullptr)
        {
            run (*previous, chunk, b.data(), len);
            for (int i = 0; i < len; ++i)
            {
                const float g = fade;
                chunk[i] = a[(size_t) i] * g + b[(size_t) i] * (1.0f - g);
                fade = juce::jmin (1.0f, fade + fadeStep);
            }
            if (fade >= 1.0f && outbox.load() == nullptr)
            {
                outbox.store (previous);   // freed by the loading thread, never here
                previous = nullptr;
            }
        }
        else
        {
            std::copy (a.begin(), a.begin() + len, chunk);
        }
    }
}
