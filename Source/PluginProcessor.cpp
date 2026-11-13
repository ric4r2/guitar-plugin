#include "PluginProcessor.h"
#include "PluginEditor.h"

GuitarDriveProcessor::GuitarDriveProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout GuitarDriveProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    const char* names[] = { "Gain", "Warmth", "Bass", "Mid", "Treble", "Reverb", "Master" };
    const float defaults[] = { 5.0f, 3.0f, 5.0f, 5.0f, 5.0f, 2.5f, 8.0f };

    for (int strip = 0; strip < 2; ++strip)
    {
        const String prefix = strip == 0 ? "" : "Right ";

        for (int k = 0; k < 7; ++k)
            p.push_back (std::make_unique<AudioParameterFloat> (
                ParameterID { paramId (strip, knobIds[k]), 1 }, prefix + names[k],
                NormalisableRange<float> (0.0f, 10.0f, 0.01f), defaults[k],
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

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
    }

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { stereoId, 1 }, "Stereo Mode", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { soloId, 1 }, "Solo",
                                                         StringArray { "Off", "Left", "Right" }, 0));
    return { p.begin(), p.end() };
}

ChannelStrip::Params GuitarDriveProcessor::paramsFor (int strip) const
{
    auto get = [this, strip] (const char* base) { return apvts.getRawParameterValue (paramId (strip, base)); };
    ChannelStrip::Params p;
    p.gain = get (knobIds[0]);
    p.warmth = get (knobIds[1]);
    p.bass = get (knobIds[2]);
    p.mid = get (knobIds[3]);
    p.treble = get (knobIds[4]);
    p.reverb = get (knobIds[5]);
    p.master = get (knobIds[6]);
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

    scratch.setSize (numScratch, samplesPerBlock, false, false, true);

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
    wasStereo = param (stereoId) > 0.5f;
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
    const bool stereo = param (stereoId) > 0.5f;

    // Take copies of the inputs: the output is written back into the same buffer.
    float* left = scratch.getWritePointer (in0);
    float* right = scratch.getWritePointer (in1);
    juce::FloatVectorOperations::copy (left, buffer.getReadPointer (0), n);
    juce::FloatVectorOperations::copy (right, buffer.getReadPointer (numIn > 1 ? 1 : 0), n);

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
                chanPan[c].setCurrentAndTargetValue (param (paramId (c, panId)));
            }
        }
        switchFade.setCurrentAndTargetValue (0.0f);   // fade the new mode in instead of clicking
        switchFade.setTargetValue (1.0f);
    }

    float* s0L = scratch.getWritePointer (out0L);
    float* s0R = scratch.getWritePointer (out0R);
    float* s1L = scratch.getWritePointer (out1L);
    float* s1R = scratch.getWritePointer (out1R);

    strips[0].process (left, s0L, s0R, n);
    if (stereo)
        strips[1].process (right, s1L, s1R, n);

    muteSmooth.setTargetValue (tuning ? 0.0f : 1.0f);

    // Solo / mute are linked: soloing one side mutes the other. A soloed side is sent to both
    // speakers (mono, ignoring its pan); with nothing soloed each side sits at its pan position.
    const int solo = stereo ? (int) std::lround (param (soloId)) : 0;
    for (int c = 0; c < 2; ++c)
    {
        chanGain[c].setTargetValue (solo == 0 || solo == c + 1 ? 1.0f : 0.0f);
        chanCenter[c].setTargetValue (solo == c + 1 ? 1.0f : 0.0f);
        chanPan[c].setTargetValue (param (paramId (c, panId)));
    }

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* sL[2] = { s0L, s1L };
    const float* sR[2] = { s0R, s1R };
    constexpr float quarterPi = juce::MathConstants<float>::pi * 0.25f;

    for (int i = 0; i < n; ++i)
    {
        const float fade = switchFade.getNextValue() * muteSmooth.getNextValue();
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
                const float theta = (chanPan[c].getNextValue() + 1.0f) * quarterPi;   // equal-power pan
                const float panL = std::cos (theta) * juce::MathConstants<float>::sqrt2;
                const float panR = std::sin (theta) * juce::MathConstants<float>::sqrt2;
                l += g * (panL + center * (1.0f - panL)) * sL[c][i];
                r += g * (panR + center * (1.0f - panR)) * sR[c][i];
            }
        }

        if (outR != nullptr)
        {
            outL[i] = l * fade;
            outR[i] = r * fade;
        }
        else
        {
            outL[i] = 0.5f * (l + r) * fade;
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
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarDriveProcessor();
}
