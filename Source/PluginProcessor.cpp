#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#include "NAM/dsp.h"

namespace
{
    using ParamList = std::vector<std::unique_ptr<juce::RangedAudioParameter>>;

    void addKnob (ParamList& p, const juce::String& id, const juce::String& name, const Params::KnobDef& k)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (k.min, k.max, k.step), k.def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([k] (float v, int) { return Params::format (k, v); })));
    }

    void addSwitch (ParamList& p, const juce::String& id, const juce::String& name, bool def)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    }
}

GuitarDriveProcessor::GuitarDriveProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    inputPtr = raw (Params::inputKnob().id);
    outputPtr = raw (Params::outputKnob().id);
    metroOnPtr = raw (Params::metroOn);
    metroBpmPtr = raw (Params::metroBpm);
    metroBeatsPtr = raw (Params::metroBeats);
    metroNotePtr = raw (Params::metroNote);
    metroVolPtr = raw (Params::metroVol);

    // recorded cabinets, spring tank and NAM captures, embedded when this build has them (see tools/pack_*.py)
   #if GUITARDRIVE_IR_PACK
    irPack = std::make_unique<IRPack> (BinaryData::irs_bin, (size_t) BinaryData::irs_binSize);
    if (! irPack->isValid())
        irPack.reset();
   #endif
   #if GUITARDRIVE_NAM_PACK
    namPack = std::make_unique<NamPack> (BinaryData::nam_bin, (size_t) BinaryData::nam_binSize);
    if (! namPack->isValid())
        namPack.reset();
   #endif
    if (irPack != nullptr || namPack != nullptr)
        loader = std::make_unique<Loader> (*this);

    // a new instance starts on your preset named "Default", if you have one (a saved session overrides it)
    if (presets.names().contains ("Default"))
        presets.load ("Default");
}

GuitarDriveProcessor::~GuitarDriveProcessor()
{
    loader.reset();   // stop loading before anything it uses goes away
}

juce::AudioProcessorValueTreeState::ParameterLayout GuitarDriveProcessor::createLayout()
{
    using namespace juce;
    ParamList p;

    for (const auto& k : Params::stripKnobs())
        addKnob (p, k.id, k.label, k);

    // the three amp heads
    for (int h = 0; h < Params::numHeads; ++h)
    {
        const String title (Params::headTitle (h));
        addSwitch (p, Params::headId (h, "on"), title + " On", true);
        for (const auto& k : Params::headKnobs (h))
            addKnob (p, Params::headId (h, k.id), title + " " + String (k.label).toLowerCase(), k);
    }

    // pedals before the amps, and the effects after the equalizer
    addSwitch (p, Params::wahOn, "Wah On", false);
    addKnob (p, Params::wahPosKnob().id, "Wah Position", Params::wahPosKnob());

    const std::vector<Params::PedalDef> wahAuto { Params::wahAutoPedal() };
    for (const auto* pedals : { &wahAuto, &Params::prePedals(), &Params::postPedals() })
        for (const auto& pedal : *pedals)
        {
            addSwitch (p, pedal.onId, String (pedal.title) + " On", false);
            for (const auto& k : pedal.knobs)
                addKnob (p, k.id, String (pedal.title) + " " + k.label, k);
        }

    // equalizer
    addSwitch (p, Params::eqOn, "EQ On", true);
    for (int b = 0; b < Params::numEqBands; ++b)
        addKnob (p, Params::eqId (b), "EQ " + String (Params::eqLabel (b)),
                 { Params::eqId (b), Params::eqLabel (b), -12.0f, 12.0f, 0.0f, 0.1f, " dB", 1 });

    addSwitch (p, Params::autoLevel, "Auto Level", true);
    addKnob (p, Params::inputKnob().id, "Input", Params::inputKnob());
    addKnob (p, Params::outputKnob().id, "Output", Params::outputKnob());

    addSwitch (p, Params::metroOn, "Metronome On", false);
    addKnob (p, Params::metroBpm, "Metronome BPM", { Params::metroBpm, "BPM", 30.0f, 260.0f, 100.0f, 1.0f, "", 0 });
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { Params::metroBeats, 1 }, "Metronome Beats", Metronome::minBeats, Metronome::maxBeats, 4));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { Params::metroNote, 1 }, "Metronome Note Value",
                                                         StringArray { "2", "4", "8", "16" }, 1));
    addKnob (p, Params::metroVol, "Metronome Volume", { Params::metroVol, "VOL", 0.0f, 10.0f, 5.0f, 0.01f });
    return { p.begin(), p.end() };
}

// ---------------------------------------------------------------- captures

int GuitarDriveProcessor::dumbleIndexFor (float knob) const
{
    const int n = namPack != nullptr ? namPack->count (NamPack::dumble) : 0;
    return n <= 1 ? 0 : juce::jlimit (0, n - 1, (int) std::lround (knob / 10.0f * (float) (n - 1)));
}

int GuitarDriveProcessor::ts9IndexFor (float driveKnob) const
{
    // the capture whose drive setting is nearest the DRIVE knob
    const int n = namPack != nullptr ? namPack->count (NamPack::ts9) : 0;
    int best = 0;
    for (int i = 1; i < n; ++i)
        if (std::abs (namPack->order (NamPack::ts9, i) - driveKnob) < std::abs (namPack->order (NamPack::ts9, best) - driveKnob))
            best = i;
    return best;
}

int GuitarDriveProcessor::sd1IndexFor (float driveKnob) const
{
    // DRIVE sweeps the captures in order: the three boosts, then drive I to VI
    const int n = namPack != nullptr ? namPack->count (NamPack::sd1) : 0;
    return n <= 1 ? 0 : juce::jlimit (0, n - 1, (int) std::lround (driveKnob / 10.0f * (float) (n - 1)));
}

juce::String GuitarDriveProcessor::dumbleVoiceName (float knob) const
{
    if (! hasDumble())
        return {};
    return namPack->name (NamPack::dumble, dumbleIndexFor (knob)).replace ("_", " ");
}

juce::String GuitarDriveProcessor::ts9CaptureName (float driveKnob) const
{
    return hasTs9() ? namPack->name (NamPack::ts9, ts9IndexFor (driveKnob)) : juce::String();
}

GuitarDriveProcessor::Loader::Loader (GuitarDriveProcessor& p) : juce::Thread ("IR and capture loader"), owner (p)
{
    startThread (juce::Thread::Priority::low);
}

GuitarDriveProcessor::Loader::~Loader()
{
    stopThread (10000);
}

void GuitarDriveProcessor::Loader::run()
{
    int lastPrepare = -1, lastDwell = -1, lastTone = -1, lastDumble = -1, lastTs9 = -1, lastSd1 = -1;
    std::array<float, Params::numHeads> lastMic { -1.0f, -1.0f, -1.0f };
    auto& strip = owner.strip;

    while (! threadShouldExit())
    {
        const int prepared = strip.getPrepareCount();
        if (prepared > 0)   // nothing to do before the first prepareToPlay
        {
            const bool fresh = prepared != lastPrepare;   // prepare() put the built-in IRs back and cleared the captures
            lastPrepare = prepared;

            if (owner.irPack != nullptr)
            {
                for (int h = 0; h < Params::numHeads; ++h)
                {
                    const float mic = owner.raw (Params::headId (h, "mic"))->load();
                    if (fresh || std::abs (mic - lastMic[(size_t) h]) > 0.02f)
                    {
                        juce::AudioBuffer<float> ir;
                        double rate = 0.0;
                        // the Dumble plays through the Two-Rock 2x12
                        if (owner.irPack->cabinet (h == 0 ? 0 : 1, mic, ir, rate))
                            strip.postIR (h, std::move (ir), rate);
                        lastMic[(size_t) h] = mic;
                    }
                }
                const int dwell = (int) std::lround (owner.raw ("rev_dwell")->load());
                const int tone = (int) std::lround (owner.raw ("rev_tone")->load());
                if (fresh || dwell != lastDwell || tone != lastTone)
                {
                    juce::AudioBuffer<float> ir;
                    double rate = 0.0;
                    if (owner.irPack->springTank (dwell, tone, ir, rate))
                        strip.postIR (ChannelStrip::springTank, std::move (ir), rate);
                    lastDwell = dwell;
                    lastTone = tone;
                }
            }

            if (owner.namPack != nullptr)
            {
                const auto loadInto = [&] (NamPlayer& player, int group, int index)
                {
                    if (auto model = owner.namPack->load (group, index, strip.getSampleRate(), strip.getMaxBlockSize()))
                    {
                        float inGain = 1.0f, outGain = 1.0f;
                        namLevels (*model, inGain, outGain, group == NamPack::dumble);   // pedals keep their own level
                        player.post (std::move (model), inGain, outGain);
                    }
                };
                const int dumble = owner.dumbleIndexFor (owner.raw (Params::headId (2, "voice"))->load());
                if (fresh || dumble != lastDumble)
                {
                    loadInto (strip.dumblePlayer(), NamPack::dumble, dumble);
                    lastDumble = dumble;
                }
                const int ts9 = owner.ts9IndexFor (owner.raw ("ts_drive")->load());
                if (owner.hasTs9() && (fresh || ts9 != lastTs9))
                {
                    loadInto (strip.ts9Player(), NamPack::ts9, ts9);
                    lastTs9 = ts9;
                }
                const int sd1 = owner.sd1IndexFor (owner.raw ("od_drive")->load());
                if (owner.hasSd1() && (fresh || sd1 != lastSd1))
                {
                    loadInto (strip.sd1Player(), NamPack::sd1, sd1);
                    lastSd1 = sd1;
                }
            }
        }
        strip.collectGarbage();
        wait (40);
    }
}

ChannelStrip::Params GuitarDriveProcessor::stripParams() const
{
    ChannelStrip::Params p;
    p.gate = raw ("gate");
    p.trans = raw ("trans");
    p.reverb = raw ("reverb");
    p.autoLevel = raw (Params::autoLevel);
    for (int h = 0; h < Params::numHeads; ++h)
    {
        auto& hp = p.heads[(size_t) h];
        hp.on = raw (Params::headId (h, "on"));
        hp.volume = raw (Params::headId (h, "volume"));
        hp.treble = raw (Params::headId (h, "treble"));
        hp.mid = raw (Params::headId (h, "mid"));
        hp.bass = raw (Params::headId (h, "bass"));
        hp.master = raw (Params::headId (h, "master"));
    }

    p.wahOn = raw ("wah_on");   p.wahAuto = raw ("wah_auto");   p.wahPos = raw ("wah_pos");
    p.wahSens = raw ("wah_sens");   p.wahQ = raw ("wah_q");
    p.compOn = raw ("comp_on");   p.compSustain = raw ("comp_sustain");   p.compAttack = raw ("comp_attack");
    p.compLevel = raw ("comp_level");
    p.tsOn = raw ("ts_on");   p.tsDrive = raw ("ts_drive");   p.tsTone = raw ("ts_tone");   p.tsLevel = raw ("ts_level");
    p.odOn = raw ("od_on");   p.odDrive = raw ("od_drive");   p.odTone = raw ("od_tone");   p.odLevel = raw ("od_level");

    p.eqOn = raw (Params::eqOn);
    for (int b = 0; b < Params::numEqBands; ++b)
        p.eq[(size_t) b] = raw (Params::eqId (b));

    auto& q = p.post;
    q.phOn = raw ("ph_on");   q.phRate = raw ("ph_rate");   q.phDepth = raw ("ph_depth");   q.phFb = raw ("ph_fb");   q.phMix = raw ("ph_mix");
    q.chOn = raw ("ch_on");   q.chRate = raw ("ch_rate");   q.chDepth = raw ("ch_depth");   q.chMix = raw ("ch_mix");
    q.trOn = raw ("tr_on");   q.trRate = raw ("tr_rate");   q.trDepth = raw ("tr_depth");
    q.dlOn = raw ("dl_on");   q.dlTime = raw ("dl_time");   q.dlFb = raw ("dl_fb");   q.dlTone = raw ("dl_tone");   q.dlMix = raw ("dl_mix");
    q.rvOn = raw ("rv_on");   q.rvSize = raw ("rv_size");   q.rvDamp = raw ("rv_damp");   q.rvMix = raw ("rv_mix");
    return p;
}

bool GuitarDriveProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in  = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    const auto ok = [] (const juce::AudioChannelSet& s) { return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo(); };
    return ok (in) && ok (out);
}

void GuitarDriveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    strip.setParams (stripParams());
    strip.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (strip.getLatencySamples());

    metronome.prepare (sampleRate);
    scratch.setSize (numScratch, samplesPerBlock, false, false, true);

    inputGain.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue (fx::dbToGain (inputPtr->load()));
    outputGain.setCurrentAndTargetValue (fx::dbToGain (outputPtr->load()));
    muteSmooth.reset (sampleRate, 0.02);
    muteSmooth.setCurrentAndTargetValue (1.0f);
    limiterGain = 1.0f;
    limiterRelease = (float) (1.0 - std::exp (-1.0 / (0.15 * sampleRate)));

    decim = std::max (1, (int) std::lround (sampleRate / 24000.0));   // analyse at about 24 kHz
    tunerRate.store ((float) (sampleRate / decim));
    decimCount = 0;
    decimSum = 0.0f;
    tunerWasActive = false;
}

void GuitarDriveProcessor::ensureScratch (int n)
{
    if (scratch.getNumSamples() < n)
        scratch.setSize (numScratch, n, false, false, true);
}

void GuitarDriveProcessor::feedTuner (const float* mono, int n)
{
    if (! tunerWasActive)
    {
        tunerFilled.store (0);
        decimCount = 0;
        decimSum = 0.0f;
        tunerWasActive = true;
    }

    int w = tunerWrite.load (std::memory_order_relaxed);
    int filled = tunerFilled.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        decimSum += mono[i];
        if (++decimCount >= decim)
        {
            tunerRing[(size_t) w] = decimSum / (float) decim;
            w = (w + 1) & (tunerRingSize - 1);
            filled = std::min (filled + 1, tunerRingSize);
            decimCount = 0;
            decimSum = 0.0f;
        }
    }
    tunerWrite.store (w, std::memory_order_release);
    tunerFilled.store (filled, std::memory_order_release);
}

bool GuitarDriveProcessor::getTunerFrame (float* dest, double& sampleRate) const
{
    constexpr int len = PitchDetector::frameSize;
    if (tunerFilled.load (std::memory_order_acquire) < len)
        return false;

    const int w = tunerWrite.load (std::memory_order_acquire);
    for (int i = 0; i < len; ++i)
        dest[i] = tunerRing[(size_t) ((w - len + i + tunerRingSize) & (tunerRingSize - 1))];
    sampleRate = (double) tunerRate.load();
    return true;
}

void GuitarDriveProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    if (n == 0 || numIn == 0 || numOut == 0)
    {
        buffer.clear();
        return;
    }

    ensureScratch (n);

    // the guitar on input 1; the output is written back into the same buffer, so take a copy first
    float* guitar = scratch.getWritePointer (inMono);
    juce::FloatVectorOperations::copy (guitar, buffer.getReadPointer (0), n);

    inputGain.setTargetValue (fx::dbToGain (inputPtr->load()));
    for (int i = 0; i < n; ++i)
        guitar[i] *= inputGain.getNextValue();
    {
        const auto range = juce::FloatVectorOperations::findMinAndMax (guitar, n);
        inputPeak.store (std::max (inputPeak.load(), std::max (std::abs (range.getStart()), std::abs (range.getEnd()))));
    }

    const bool tuning = tunerActive.load();
    if (tuning)
        feedTuner (guitar, n);
    else
        tunerWasActive = false;

    float* l = scratch.getWritePointer (outL);
    float* r = scratch.getWritePointer (outR);
    strip.process (guitar, l, r, n);

    metronome.process (l, r, n, metroOnPtr->load() > 0.5f, metroBpmPtr->load(),
                       (int) std::lround (metroBeatsPtr->load()), (int) std::lround (metroNotePtr->load()), metroVolPtr->load());

    // output volume and tuner mute, then out
    muteSmooth.setTargetValue (tuning ? 0.0f : 1.0f);
    outputGain.setTargetValue (fx::dbToGain (outputPtr->load()));

    float* dstL = buffer.getWritePointer (0);
    float* dstR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    float outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float gain = muteSmooth.getNextValue() * outputGain.getNextValue();
        float a = l[i] * gain, b = r[i] * gain;

        // safety limiter: nothing leaves above -1 dBFS; it reacts at once and lets go over 150 ms
        constexpr float ceiling = 0.89f;
        const float peak = std::max (std::abs (a), std::abs (b));
        if (peak * limiterGain > ceiling)
            limiterGain = ceiling / peak;
        else
            limiterGain += limiterRelease * (1.0f - limiterGain);
        a *= limiterGain;
        b *= limiterGain;
        outPeak = std::max (outPeak, std::max (std::abs (a), std::abs (b)));
        if (dstR != nullptr)
        {
            dstL[i] = a;
            dstR[i] = b;
        }
        else
        {
            dstL[i] = 0.5f * (a + b);
        }
    }
    outputPeak.store (std::max (outputPeak.load(), outPeak));
}

juce::AudioProcessorEditor* GuitarDriveProcessor::createEditor()
{
    return new GuitarDriveEditor (*this);
}

void GuitarDriveProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void GuitarDriveProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            presets.restoreFromState();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarDriveProcessor();
}
