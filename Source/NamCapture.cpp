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

int namRateFactor (double hostSampleRate)
{
    return hostSampleRate >= 160000.0 ? 4 : hostSampleRate >= 80000.0 ? 2 : 1;
}

void RateBridge::Stage::design()
{
    // windowed sinc with its cut-off at a quarter of the higher rate (the lower rate's Nyquist), Blackman window
    const int c = taps / 2;
    for (int k = 0; k < taps; ++k)
    {
        const double x = 0.5 * (k - c);
        const double sinc = k == c ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
        const double w = 0.42 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * k / (taps - 1))
                       + 0.08 * std::cos (2.0 * juce::MathConstants<double>::twoPi * k / (taps - 1));
        h[(size_t) k] = (float) (0.5 * sinc * w);
    }
    double sum = 0.0;
    for (auto v : h)
        sum += v;
    for (auto& v : h)
        v = (float) (v / sum);   // unity gain at DC
}

void RateBridge::Stage::clear()
{
    downZ.fill (0.0f);
    upZ.fill (0.0f);
    downPos = upPos = phase = 0;
}

int RateBridge::Stage::decimate (const float* in, int n, float* out)
{
    int m = 0;
    for (int i = 0; i < n; ++i)
    {
        downZ[(size_t) downPos] = downZ[(size_t) (downPos + taps)] = in[i];
        downPos = (downPos + 1) % taps;
        phase ^= 1;
        if (phase == 0)   // every second sample: one filtered output
        {
            const float* z = downZ.data() + downPos;   // oldest to newest
            float s = 0.0f;
            for (int k = 0; k < taps; ++k)
                s += h[(size_t) k] * z[taps - 1 - k];
            out[m++] = s;
        }
    }
    return m;
}

int RateBridge::Stage::interpolate (const float* in, int n, float* out)
{
    int m = 0;
    for (int i = 0; i < n; ++i)
        for (int half = 0; half < 2; ++half)   // the sample (doubled, to keep the level), then a zero, both filtered
        {
            upZ[(size_t) upPos] = upZ[(size_t) (upPos + taps)] = half == 0 ? 2.0f * in[i] : 0.0f;
            upPos = (upPos + 1) % taps;
            const float* z = upZ.data() + upPos;
            float s = 0.0f;
            for (int k = 0; k < taps; ++k)
                s += h[(size_t) k] * z[taps - 1 - k];
            out[m++] = s;
        }
    return m;
}

void RateBridge::prepare (int newFactor, int maxBlockSize)
{
    factor = juce::jmax (1, newFactor);
    maxBlock = juce::jmax (1, maxBlockSize);
    stages.assign (factor >= 4 ? 2 : factor >= 2 ? 1 : 0, Stage {});
    levels.clear();
    int size = maxBlock;
    for (auto& s : stages)
    {
        s.design();
        size = size / 2 + 2;
        levels.emplace_back ((size_t) size, 0.0f);
    }
    if (levels.empty())
        levels.emplace_back ((size_t) maxBlock, 0.0f);
    upOut.assign ((size_t) (maxBlock + 8 * factor), 0.0f);
    fifo.assign ((size_t) (2 * maxBlock + 16 * factor), 0.0f);
    reset();
}

void RateBridge::reset()
{
    for (auto& s : stages)
        s.clear();
    std::fill (fifo.begin(), fifo.end(), 0.0f);
    // The output comes in whole low-rate samples, so up to factor - 1 samples can be missing at the end of a block:
    // start that far behind. With the filters' own delay that makes a fixed latency.
    fifoRead = 0;
    fifoWrite = factor - 1;
    int filterDelay = 0, scale = 1;
    for (size_t s = 0; s < stages.size(); ++s)
    {
        filterDelay += scale * (Stage::taps - 1);   // half the taps on the way down, half on the way up
        scale *= 2;
    }
    latencySamples = stages.empty() ? 0 : filterDelay;   // measured: the start-up offset adds no delay
}

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
    hostRate = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);
    bridge.prepare (namRateFactor (sampleRate), maxBlock);
    const int inner = modelBlockSize();
    in.assign ((size_t) inner, 0.0f);
    a.assign ((size_t) inner, 0.0f);
    b.assign ((size_t) inner, 0.0f);
    fadeStep = (float) (1.0 / (0.05 * modelRate()));   // 50 ms cross-fade
    // models built for the old settings are useless now; the loading thread sends new ones after prepare()
    delete current;
    delete previous;
    current = previous = nullptr;
    fade = 1.0f;
}

void NamPlayer::reset()
{
    bridge.reset();
    fade = 1.0f;
    if (previous != nullptr && outbox.load() == nullptr)
    {
        outbox.store (previous);
        previous = nullptr;
    }
}

int namDelay (nam::DSP& model, double sampleRate, int maxBlockSize, float inputGain)
{
    const int n = 8192, start = 512, burst = 4096, maxLag = 400;
    juce::Random rng (1234);
    std::vector<float> src ((size_t) n, 0.0f), in ((size_t) n, 0.0f), out ((size_t) n, 0.0f);
    for (int i = 0; i < burst; ++i)
        src[(size_t) (start + i)] = 0.02f * (rng.nextFloat() * 2.0f - 1.0f);   // clean-guitar level
    for (int i = 0; i < n; ++i)
        in[(size_t) i] = src[(size_t) i] * inputGain;
    const int block = juce::jmax (1, maxBlockSize);
    for (int s = 0; s < n; s += block)
    {
        float* ins[1] = { in.data() + s };
        float* outs[1] = { out.data() + s };
        model.process (ins, outs, juce::jmin (block, n - s));
    }
    int best = 0;
    double bestC = -1.0;
    for (int lag = 0; lag < maxLag; ++lag)
    {
        double c = 0.0;
        for (int i = start; i < start + burst && i + lag < n; ++i)
            c += (double) src[(size_t) i] * out[(size_t) (i + lag)];
        if (std::abs (c) > bestC)
        {
            bestC = std::abs (c);
            best = lag;
        }
    }
    model.Reset (sampleRate, maxBlockSize);
    return best;
}

void NamPlayer::post (std::unique_ptr<nam::DSP> model, float inputGain, float outputGain, int delay)
{
    auto* slot = new Slot { std::move (model), inputGain, outputGain, delay };
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

    if (bridge.getFactor() > 1)
    {
        for (int start = 0; start < n; start += maxBlock)   // the bridge's buffers fit maxBlock samples
            bridge.process (x + start, juce::jmin (maxBlock, n - start), [this] (float* d, int m) { processNative (d, m); });
    }
    else
    {
        processNative (x, n);
    }
}

void NamPlayer::processNative (float* x, int n)
{
    const int block = (int) a.size();
    for (int start = 0; start < n; start += block)
    {
        const int len = juce::jmin (block, n - start);
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
