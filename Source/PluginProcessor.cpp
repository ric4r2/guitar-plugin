#include "PluginProcessor.h"

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
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive",
                                                        NormalisableRange<float> (0.0f, 40.0f, 0.1f), 12.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone", 1 }, "Tone (Hz)",
                                                        NormalisableRange<float> (500.0f, 8000.0f, 1.0f, 0.4f), 3000.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "level", 1 }, "Level (dB)",
                                                        NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -6.0f));
    return { p.begin(), p.end() };
}

bool GuitarDriveProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    // Guitar interface input is usually mono; accept mono or stereo, same in/out.
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void GuitarDriveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                        (juce::uint32) getTotalNumOutputChannels() };
    toneFilter.prepare (spec);
    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    driveSmooth.reset (sampleRate, 0.02);
    levelSmooth.reset (sampleRate, 0.02);
    driveSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("drive")->load()));
    levelSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("level")->load()));

    setLatencySamples ((int) oversampling.getLatencyInSamples());
}

void GuitarDriveProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numCh = buffer.getNumChannels();
    for (int ch = getTotalNumInputChannels(); ch < numCh; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    driveSmooth.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("drive")->load()));
    levelSmooth.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("level")->load()));
    *toneFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
        sr, apvts.getRawParameterValue ("tone")->load());

    juce::dsp::AudioBlock<float> block (buffer);

    // Waveshaping at 2x oversampling to tame aliasing.
    auto up = oversampling.processSamplesUp (block);
    const auto upSamples = up.getNumSamples();
    for (size_t i = 0; i < upSamples; ++i)
    {
        const float g = driveSmooth.getNextValue();   // advances once per oversampled sample
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* d = up.getChannelPointer (ch);
            d[i] = std::tanh (d[i] * g);
        }
    }
    oversampling.processSamplesDown (block);

    toneFilter.process (juce::dsp::ProcessContextReplacing<float> (block));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float lv = levelSmooth.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer (ch)[i] *= lv;
    }
}

juce::AudioProcessorEditor* GuitarDriveProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
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
