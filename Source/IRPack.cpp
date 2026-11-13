#include "IRPack.h"
#include "CabinetIR.h"
#include <juce_core/juce_core.h>

IRPack::IRPack (const void* data, size_t size)
{
    if (data == nullptr || size == 0)
        return;

    juce::MemoryInputStream compressed (data, size, false);
    juce::GZIPDecompressorInputStream unzip (&compressed, false, juce::GZIPDecompressorInputStream::zlibFormat);
    juce::MemoryBlock all;
    unzip.readIntoMemoryBlock (all);

    juce::MemoryInputStream in (all, false);
    char magic[4] {};
    if (in.read (magic, 4) != 4 || std::memcmp (magic, "JGIR", 4) != 0)
        return;
    const auto version = (juce::uint32) in.readInt();
    const auto count = (juce::uint32) in.readInt();
    if (version != 1 || count == 0 || count > 10000)
        return;

    std::vector<Entry> parsed (count);
    for (auto& e : parsed)
    {
        e.group = (juce::uint32) in.readInt();
        e.a = (juce::uint32) in.readInt();
        e.b = (juce::uint32) in.readInt();
        e.rate = (juce::uint32) in.readInt();
        e.length = (juce::uint32) in.readInt();
        e.format = (juce::uint32) in.readInt();
        e.scale = in.readFloat();
        e.offset = (juce::uint32) in.readInt();
    }

    const auto dataStart = (size_t) in.getPosition();
    const size_t dataSize = all.getSize() - dataStart;
    for (const auto& e : parsed)
    {
        const size_t bytes = (size_t) e.length * (e.format == 1 ? 4u : 2u);
        if ((size_t) e.offset + bytes > dataSize || e.rate < 8000 || e.length == 0 || (e.format != 1 && e.format != 2))
            return;   // damaged pack: use nothing from it
    }

    samples.append (static_cast<const char*> (all.getData()) + dataStart, dataSize);
    entries = std::move (parsed);
}

const IRPack::Entry* IRPack::find (juce::uint32 group, juce::uint32 a, juce::uint32 b) const
{
    for (const auto& e : entries)
        if (e.group == group && e.a == a && e.b == b)
            return &e;
    return nullptr;
}

void IRPack::decode (const Entry& e, float* dest, int length) const
{
    const auto* base = static_cast<const char*> (samples.getData()) + e.offset;
    const int n = juce::jmin (length, (int) e.length);
    for (int i = 0; i < n; ++i)
    {
        if (e.format == 1)
        {
            float v;
            std::memcpy (&v, base + 4 * i, 4);
            dest[i] = v;
        }
        else
        {
            juce::int16 v;
            std::memcpy (&v, base + 2 * i, 2);
            dest[i] = (float) v * e.scale;
        }
    }
    for (int i = n; i < length; ++i)
        dest[i] = 0.0f;
}

bool IRPack::cabinet (int model, float micKnob, juce::AudioBuffer<float>& out, double& rate) const
{
    // the knob sweeps mic 0 -> 1 -> 2; between two positions the IRs are cross-faded (they were aligned when packed)
    const float pos = juce::jlimit (0.0f, 2.0f, micKnob / 5.0f);
    const int lo = juce::jmin (1, (int) pos);
    const float frac = pos - (float) lo;
    const auto* a = find ((juce::uint32) model, (juce::uint32) lo, 0);
    const auto* b = find ((juce::uint32) model, (juce::uint32) lo + 1, 0);
    if (a == nullptr || b == nullptr || a->rate != b->rate)
        return false;

    const int length = (int) juce::jmax (a->length, b->length);
    juce::AudioBuffer<float> ia (1, length), ib (1, length);
    decode (*a, ia.getWritePointer (0), length);
    decode (*b, ib.getWritePointer (0), length);
    rate = a->rate;
    cab::normaliseAt1k (ia, rate);
    cab::normaliseAt1k (ib, rate);

    out.setSize (1, length);
    for (int i = 0; i < length; ++i)
        out.setSample (0, i, ia.getSample (0, i) * (1.0f - frac) + ib.getSample (0, i) * frac);
    cab::normaliseAt1k (out, rate);
    return true;
}

bool IRPack::springTank (int dwell, int tone, juce::AudioBuffer<float>& out, double& rate) const
{
    const auto* e = find ((juce::uint32) spring, (juce::uint32) juce::jlimit (1, 10, dwell), (juce::uint32) juce::jlimit (1, 10, tone));
    if (e == nullptr)
        return false;
    out.setSize (1, (int) e->length);
    decode (*e, out.getWritePointer (0), (int) e->length);
    rate = e->rate;
    return true;
}
