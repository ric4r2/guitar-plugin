#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    stereoPtr = raw (stereoId);
    soloPtr = raw (soloId);
    rightSamePtr = raw (rightSameId);
    inputPtr = raw (Params::inputKnob().id);
    outputPtr = raw (Params::outputKnob().id);
    for (int s = 0; s < 2; ++s)
        panPtr[s] = raw (paramId (s, panId));

    metroOnPtr = raw (Params::metroOn);
    metroBpmPtr = raw (Params::metroBpm);
    metroBeatsPtr = raw (Params::metroBeats);
    metroNotePtr = raw (Params::metroNote);
    metroVolPtr = raw (Params::metroVol);
}

juce::AudioProcessorValueTreeState::ParameterLayout GuitarDriveProcessor::createLayout()
{
    using namespace juce;
    ParamList p;

    for (int strip = 0; strip < 2; ++strip)
    {
        const String prefix = strip == 0 ? "" : "Right ";

        // amp
        for (const auto& k : Params::stripKnobs())
            addKnob (p, paramId (strip, k.id), prefix + k.label, k);

        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { paramId (strip, panId), 1 }, prefix + "Pan",
            NormalisableRange<float> (-1.0f, 1.0f, 0.01f), strip == 0 ? -1.0f : 1.0f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int)
                {
                    const int amount = (int) std::lround (std::abs (v) * 100.0f);
                    return amount == 0 ? String ("C") : String (v < 0.0f ? "L" : "R") + String (amount);
                })
                .withValueFromStringFunction ([] (const String& s)
                {
                    const auto t = s.trim().toUpperCase();
                    if (t.startsWith ("L")) return -t.substring (1).getFloatValue() / 100.0f;
                    if (t.startsWith ("R")) return  t.substring (1).getFloatValue() / 100.0f;
                    return 0.0f;
                })));

        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { paramId (strip, revTypeId), 1 }, prefix + "Reverb Type",
                                                             StringArray { "Plate", "Spring" }, 0));

        // pedals before the amp, and the effects after the equalizer
        addSwitch (p, paramId (strip, Params::wahOn), prefix + "Wah On", false);
        addKnob (p, paramId (strip, Params::wahPosKnob().id), prefix + "Wah Position", Params::wahPosKnob());

        const std::vector<Params::PedalDef> wahAuto { Params::wahAutoPedal() };
        for (const auto* pedals : { &wahAuto, &Params::prePedals(), &Params::postPedals() })
            for (const auto& pedal : *pedals)
            {
                addSwitch (p, paramId (strip, pedal.onId), prefix + pedal.title + " On", false);
                for (const auto& k : pedal.knobs)
                    addKnob (p, paramId (strip, k.id), prefix + pedal.title + " " + k.label, k);
            }

        // equalizer
        addSwitch (p, paramId (strip, Params::eqOn), prefix + "EQ On", true);
        for (int b = 0; b < Params::numEqBands; ++b)
            addKnob (p, paramId (strip, Params::eqId (b)), prefix + "EQ " + Params::eqLabel (b),
                     { Params::eqId (b), Params::eqLabel (b), -12.0f, 12.0f, 0.0f, 0.1f, " dB", 1 });
    }

    addKnob (p, Params::inputKnob().id, "Input", Params::inputKnob());
    addKnob (p, Params::outputKnob().id, "Output", Params::outputKnob());

    addSwitch (p, Params::metroOn, "Metronome On", false);
    addKnob (p, Params::metroBpm, "Metronome BPM", { Params::metroBpm, "BPM", 30.0f, 260.0f, 100.0f, 1.0f, "", 0 });
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { Params::metroBeats, 1 }, "Metronome Beats", Metronome::minBeats, Metronome::maxBeats, 4));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { Params::metroNote, 1 }, "Metronome Note Value",
                                                         StringArray { "2", "4", "8", "16" }, 1));
    addKnob (p, Params::metroVol, "Metronome Volume", { Params::metroVol, "VOL", 0.0f, 10.0f, 5.0f, 0.01f });

    addSwitch (p, stereoId, "Stereo Mode", false);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { soloId, 1 }, "Solo",
                                                         StringArray { "Off", "Left", "Right" }, 0));
    addSwitch (p, rightSameId, "Right Uses Input 1", true);
    return { p.begin(), p.end() };
}

ChannelStrip::Params GuitarDriveProcessor::paramsFor (int strip) const
{
    auto own = [this, strip] (const char* base) { return raw (paramId (strip, base)); };

    ChannelStrip::Params p;
    p.gate = own ("gate");
    p.trans = own ("trans");
    p.gain = own ("gain");
    p.warmth = own ("warmth");
    p.bass = own ("bass");
    p.mid = own ("mid");
    p.treble = own ("treble");
    p.reverb = own ("reverb");
    p.master = own ("master");
    p.revType = own (revTypeId);

    p.wahOn = own ("wah_on");   p.wahAuto = own ("wah_auto");   p.wahPos = own ("wah_pos");
    p.wahSens = own ("wah_sens");   p.wahQ = own ("wah_q");
    p.compOn = own ("comp_on");   p.compSustain = own ("comp_sustain");   p.compAttack = own ("comp_attack");
    p.compLevel = own ("comp_level");
    p.tsOn = own ("ts_on");   p.tsDrive = own ("ts_drive");   p.tsTone = own ("ts_tone");   p.tsLevel = own ("ts_level");
    p.odOn = own ("od_on");   p.odDrive = own ("od_drive");   p.odTone = own ("od_tone");   p.odLevel = own ("od_level");

    p.eqOn = own (Params::eqOn);
    for (int b = 0; b < Params::numEqBands; ++b)
        p.eq[(size_t) b] = own (Params::eqId (b));

    auto& q = p.post;
    q.phOn = own ("ph_on");   q.phRate = own ("ph_rate");   q.phDepth = own ("ph_depth");   q.phFb = own ("ph_fb");   q.phMix = own ("ph_mix");
    q.chOn = own ("ch_on");   q.chRate = own ("ch_rate");   q.chDepth = own ("ch_depth");   q.chMix = own ("ch_mix");
    q.trOn = own ("tr_on");   q.trRate = own ("tr_rate");   q.trDepth = own ("tr_depth");
    q.dlOn = own ("dl_on");   q.dlTime = own ("dl_time");   q.dlFb = own ("dl_fb");   q.dlTone = own ("dl_tone");   q.dlMix = own ("dl_mix");
    q.rvOn = own ("rv_on");   q.rvSize = own ("rv_size");   q.rvDamp = own ("rv_damp");   q.rvMix = own ("rv_mix");
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
    for (int s = 0; s < 2; ++s)
    {
        strips[s].setParams (paramsFor (s));
        strips[s].prepare (sampleRate, samplesPerBlock);
    }
    setLatencySamples (strips[0].getLatencySamples());

    metronome.prepare (sampleRate);

    scratch.setSize (numScratch, samplesPerBlock, false, false, true);

    inputGain.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue (fx::dbToGain (inputPtr->load()));
    outputGain.setCurrentAndTargetValue (fx::dbToGain (outputPtr->load()));
    muteSmooth.reset (sampleRate, 0.02);
    muteSmooth.setCurrentAndTargetValue (1.0f);
    switchFade.reset (sampleRate, 0.02);
    switchFade.setCurrentAndTargetValue (1.0f);
    for (int c = 0; c < 2; ++c)
    {
        chanGain[c].reset (sampleRate, 0.02);
        chanCenter[c].reset (sampleRate, 0.02);
        chanPan[c].reset (sampleRate, 0.03);
    }
    wasStereo = stereoPtr->load() > 0.5f;
    if (wasStereo)
        strips[1].reset();

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
    const bool stereo = stereoPtr->load() > 0.5f;

    // Take copies of the inputs: the output is written back into the same buffer.
    float* left = scratch.getWritePointer (in0);
    float* right = scratch.getWritePointer (in1);
    juce::FloatVectorOperations::copy (left, buffer.getReadPointer (0), n);
    // With one guitar on input 1 the right channel would otherwise listen to an empty input 2 and stay silent,
    // so by default both channels take input 1 (two independent chains from the same guitar).
    const bool rightFromLeft = rightSamePtr->load() > 0.5f;
    juce::FloatVectorOperations::copy (right, buffer.getReadPointer (numIn > 1 && ! rightFromLeft ? 1 : 0), n);

    // Input volume: plain gain, before everything else.
    inputGain.setTargetValue (fx::dbToGain (inputPtr->load()));
    for (int i = 0; i < n; ++i)
    {
        const float g = inputGain.getNextValue();
        left[i] *= g;
        right[i] *= g;
    }

    const bool tuning = tunerActive.load();
    if (tuning)
    {
        // In stereo mode listen to whichever input is louder.
        const float* source = left;
        if (stereo)
        {
            const float e0 = juce::FloatVectorOperations::findMaximum (left, n);
            const float e1 = juce::FloatVectorOperations::findMaximum (right, n);
            source = e1 > e0 ? right : left;
        }
        feedTuner (source, n);
    }
    else
    {
        tunerWasActive = false;
    }

    if (stereo != wasStereo)
    {
        wasStereo = stereo;
        if (stereo)
        {
            strips[1].reset();
            for (int c = 0; c < 2; ++c)
            {
                chanGain[c].setCurrentAndTargetValue (1.0f);
                chanCenter[c].setCurrentAndTargetValue (0.0f);
                chanPan[c].setCurrentAndTargetValue (panPtr[c]->load());
            }
        }
        switchFade.setCurrentAndTargetValue (0.0f);   // fade the new mode in instead of clicking
        switchFade.setTargetValue (1.0f);
    }

    float* s0L = scratch.getWritePointer (out0L);
    float* s0R = scratch.getWritePointer (out0R);
    float* s1L = scratch.getWritePointer (out1L);
    float* s1R = scratch.getWritePointer (out1R);

    // each channel runs its whole chain: pedals, amp, equalizer and effects
    strips[0].process (left, s0L, s0R, n);
    if (stereo)
        strips[1].process (right, s1L, s1R, n);

    // Solo / mute are linked: soloing one side mutes the other. A soloed side is sent to both
    // speakers (mono, ignoring its pan); with nothing soloed each side sits at its pan position.
    const int solo = stereo ? (int) std::lround (soloPtr->load()) : 0;
    for (int c = 0; c < 2; ++c)
    {
        chanGain[c].setTargetValue (solo == 0 || solo == c + 1 ? 1.0f : 0.0f);
        chanCenter[c].setTargetValue (solo == c + 1 ? 1.0f : 0.0f);
        chanPan[c].setTargetValue (panPtr[c]->load());
    }

    float* mixLp = scratch.getWritePointer (mixL);
    float* mixRp = scratch.getWritePointer (mixR);
    const float* sL[2] = { s0L, s1L };
    const float* sR[2] = { s0R, s1R };
    constexpr float quarterPi = juce::MathConstants<float>::pi * 0.25f;

    for (int i = 0; i < n; ++i)
    {
        float l, r;

        if (! stereo)
        {
            l = s0L[i];
            r = s0R[i];
        }
        else
        {
            l = r = 0.0f;
            for (int c = 0; c < 2; ++c)
            {
                const float g = chanGain[c].getNextValue();
                const float center = chanCenter[c].getNextValue();
                const float theta = (chanPan[c].getNextValue() + 1.0f) * quarterPi;   // equal-power pan: hard side = unity
                const float panL = std::cos (theta);
                const float panR = std::sin (theta);
                l += g * (panL + center * (1.0f - panL)) * sL[c][i];
                r += g * (panR + center * (1.0f - panR)) * sR[c][i];
            }
        }
        mixLp[i] = l;
        mixRp[i] = r;
    }

    metronome.process (mixLp, mixRp, n, metroOnPtr->load() > 0.5f, metroBpmPtr->load(),
                       (int) std::lround (metroBeatsPtr->load()), (int) std::lround (metroNotePtr->load()), metroVolPtr->load());

    // Output volume, tuner mute and mode fade, then out.
    muteSmooth.setTargetValue (tuning ? 0.0f : 1.0f);
    outputGain.setTargetValue (fx::dbToGain (outputPtr->load()));

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        const float gain = switchFade.getNextValue() * muteSmooth.getNextValue() * outputGain.getNextValue();
        const float l = mixLp[i] * gain;
        const float r = mixRp[i] * gain;

        if (outR != nullptr)
        {
            outL[i] = l;
            outR[i] = r;
        }
        else
        {
            outL[i] = 0.5f * (l + r);
        }
    }
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
