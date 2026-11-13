#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SpringReverb.h"

class GuitarDriveProcessor : public juce::AudioProcessor
{
public:
    GuitarDriveProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GuitarDrive"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // All knobs run 0..10.
    static constexpr const char* gainId   = "gain";
    static constexpr const char* bassId   = "bass";
    static constexpr const char* midId    = "mid";
    static constexpr const char* trebleId = "treble";
    static constexpr const char* reverbId = "reverb";
    static constexpr const char* masterId = "master";

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "STATE", createLayout() };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void updateToneStack();

    using Filter = juce::dsp::IIR::Filter<float>;
    Filter bassFilter, midFilter, trebleFilter;
    float lastBass = -1.0f, lastMid = -1.0f, lastTreble = -1.0f;

    SpringReverb spring;
    juce::SmoothedValue<float> gainSmooth, reverbSmooth, masterSmooth;

    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDriveProcessor)
};
