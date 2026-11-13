#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    float gainToLinear (float knob)   { return juce::Decibels::decibelsToGain (juce::jmap (knob, 0.0f, 10.0f, -24.0f, 24.0f)); }
    float masterToLinear (float knob) { return knob < 0.05f ? 0.0f : juce::Decibels::decibelsToGain (juce::jmap (knob, 0.0f, 10.0f, -40.0f, 10.0f)); }
    float reverbToLinear (float knob) { return std::pow (knob / 10.0f, 1.5f); }
    float eqKnobToDb (float knob)     { return (knob - 5.0f) * 2.4f; }   // 5 = flat, +/-12 dB at the ends
}

GuitarDriveProcessor::GuitarDriveProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

juce::AudioProcessorValueTreeState::ParameterLayout GuitarDriveProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto add = [&p] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                            NormalisableRange<float> (0.0f, 10.0f, 0.01f), def));
    };
    add (gainId,   "Gain",   5.0f);
    add (bassId,   "Bass",   5.0f);
    add (midId,    "Mid",    5.0f);
    add (trebleId, "Treble", 5.0f);
    add (reverbId, "Reverb", 2.5f);
    add (masterId, "Master", 8.0f);
    return { p.begin(), p.end() };
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
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    for (auto* f : { &bassFilter, &midFilter, &trebleFilter })
        f->prepare (spec);
    lastBass = lastMid = lastTreble = -1.0f;
    updateToneStack();

    spring.prepare (sampleRate);

    gainSmooth.reset (sampleRate, 0.03);
    reverbSmooth.reset (sampleRate, 0.03);
    masterSmooth.reset (sampleRate, 0.03);
    gainSmooth.setCurrentAndTargetValue (gainToLinear (param (gainId)));
    reverbSmooth.setCurrentAndTargetValue (reverbToLinear (param (reverbId)));
    masterSmooth.setCurrentAndTargetValue (masterToLinear (param (masterId)));
}

void GuitarDriveProcessor::updateToneStack()
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    const float bass = param (bassId), mid = param (midId), treble = param (trebleId);

    if (bass != lastBass)
    {
        bassFilter.coefficients = Coeffs::makeLowShelf (sr, 120.0f, 0.7f, juce::Decibels::decibelsToGain (eqKnobToDb (bass)));
        lastBass = bass;
    }
    if (mid != lastMid)
    {
        midFilter.coefficients = Coeffs::makePeakFilter (sr, 750.0f, 0.8f, juce::Decibels::decibelsToGain (eqKnobToDb (mid)));
        lastMid = mid;
    }
    if (treble != lastTreble)
    {
        trebleFilter.coefficients = Coeffs::makeHighShelf (sr, 3200.0f, 0.7f, juce::Decibels::decibelsToGain (eqKnobToDb (treble)));
        lastTreble = treble;
    }
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

    // The guitar is one signal. Work on channel 0 as a mono track and fan it out to every output at the end,
    // so a mono input (the ID14's instrument input) comes out of both ears.
    auto* mono = buffer.getWritePointer (0);
    if (numIn > 1)
    {
        const auto* right = buffer.getReadPointer (1);
        for (int i = 0; i < n; ++i)
            mono[i] = 0.5f * (mono[i] + right[i]);
    }

    gainSmooth.setTargetValue (gainToLinear (param (gainId)));
    reverbSmooth.setTargetValue (reverbToLinear (param (reverbId)));
    masterSmooth.setTargetValue (masterToLinear (param (masterId)));

    for (int i = 0; i < n; ++i)
        mono[i] *= gainSmooth.getNextValue();

    updateToneStack();
    float* chans[1] = { mono };
    juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    bassFilter.process (ctx);
    midFilter.process (ctx);
    trebleFilter.process (ctx);

    auto* second = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        const float dry = mono[i];
        const float wet = reverbSmooth.getNextValue();
        const float master = masterSmooth.getNextValue();
        float wl, wr;
        spring.process (dry, wl, wr);

        mono[i] = (dry + wet * wl) * master;
        if (second != nullptr)
            second[i] = (dry + wet * wr) * master;
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
