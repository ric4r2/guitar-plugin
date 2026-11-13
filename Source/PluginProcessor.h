#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ChannelStrip.h"
#include "Tuner.h"

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

    // Per-strip knobs run 0..10 (pan runs -1..1). Strip 0 is the mono / left strip, strip 1 the right strip.
    static constexpr const char* knobIds[] = { "gain", "warmth", "bass", "mid", "treble", "reverb", "master" };
    static constexpr const char* panId = "pan";
    static constexpr const char* stereoId = "stereo";   // false = mono (default), true = two strips
    static constexpr const char* soloId = "solo";       // 0 = off, 1 = left soloed (right muted), 2 = right soloed (left muted)
    static constexpr const char* rightSameId = "rightsame";   // stereo mode: true = right strip hears input 1 too, false = input 2

    static juce::String paramId (int strip, const char* base) { return strip == 0 ? juce::String (base) : "r_" + juce::String (base); }

    // Tuner: while active the output is muted and the input is analysed.
    void setTunerActive (bool on) { tunerActive.store (on); }
    bool isTunerActive() const { return tunerActive.load(); }
    // Copies the latest PitchDetector::frameSize decimated samples; false until enough have arrived.
    bool getTunerFrame (float* dest, double& sampleRate) const;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "STATE", createLayout() };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    ChannelStrip::Params paramsFor (int strip) const;
    void feedTuner (const float* mono, int n);
    void ensureScratch (int n);

    // scratch channels
    enum { in0, in1, out0L, out0R, out1L, out1R, numScratch };

    ChannelStrip strips[2];
    juce::AudioBuffer<float> scratch { numScratch, 0 };

    juce::SmoothedValue<float> muteSmooth, switchFade;
    juce::SmoothedValue<float> chanGain[2], chanCenter[2], chanPan[2];
    bool wasStereo = false;

    // tuner capture (written on the audio thread, read by the editor)
    static constexpr int tunerRingSize = 8192;   // power of two
    std::array<float, tunerRingSize> tunerRing {};
    std::atomic<int> tunerWrite { 0 }, tunerFilled { 0 };
    std::atomic<float> tunerRate { 24000.0f };
    std::atomic<bool> tunerActive { false };
    bool tunerWasActive = false;
    int decim = 2, decimCount = 0;
    float decimSum = 0.0f;

    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDriveProcessor)
};
