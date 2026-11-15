#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ChannelStrip.h"
#include "IRPack.h"
#include "Effects.h"
#include "Metronome.h"
#include "NamCapture.h"
#include "Params.h"
#include "Presets.h"
#include "Tuner.h"

class GuitarDriveProcessor : public juce::AudioProcessor
{
public:
    GuitarDriveProcessor();
    ~GuitarDriveProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Joe Garcia Rig"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // The plugin has one channel; the strip argument is kept for the views that are built per channel.
    static juce::String paramId (int, const char* base) { return juce::String (base); }

    // Tuner: while active the output is muted and the input is analysed.
    void setTunerActive (bool on) { tunerActive.store (on); }
    bool isTunerActive() const { return tunerActive.load(); }
    // Copies the latest PitchDetector::frameSize decimated samples; false until enough have arrived.
    bool getTunerFrame (float* dest, double& sampleRate) const;

    const Metronome& getMetronome() const { return metronome; }

    // NAM captures built into this copy (personal builds only)
    bool hasDumble() const { return namPack != nullptr && namPack->count (NamPack::dumble) > 0; }
    bool hasTs9() const { return namPack != nullptr && namPack->count (NamPack::ts9) > 0; }
    juce::String dumbleVoiceName (float knob) const;   // the capture the Dumble's VOICE knob points at
    juce::String ts9CaptureName (float driveKnob) const;
    int dumbleIndexFor (float knob) const;
    int ts9IndexFor (float driveKnob) const;

    // block peaks for the level meters; the editor reads and clears them
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "STATE", createLayout() };
    PresetManager presets { apvts, *this, defaultPresetFolder() };

private:
    static juce::File defaultPresetFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Joe Garcia Rig").getChildFile ("Presets");
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    const std::atomic<float>* raw (const juce::String& id) const { return apvts.getRawParameterValue (id); }
    ChannelStrip::Params stripParams() const;
    void feedTuner (const float* mono, int n);
    void ensureScratch (int n);

    enum { inMono, outL, outR, numScratch };

    ChannelStrip strip;

    // Recorded cabinets, spring tank and NAM captures (only in builds that embed them), kept in step with the MIC,
    // DWELL, TONE, VOICE and TS9 DRIVE knobs by a background thread: loading never happens on the audio thread.
    struct Loader : juce::Thread
    {
        explicit Loader (GuitarDriveProcessor& p);
        ~Loader() override;
        void run() override;
        GuitarDriveProcessor& owner;
    };
    std::unique_ptr<IRPack> irPack;
    std::unique_ptr<NamPack> namPack;
    std::unique_ptr<Loader> loader;   // declared after strip and the packs, so it stops before they go
    Metronome metronome;
    juce::AudioBuffer<float> scratch { numScratch, 0 };

    juce::SmoothedValue<float> inputGain, outputGain, muteSmooth;

    // cached parameter pointers
    const std::atomic<float>* inputPtr = nullptr;
    const std::atomic<float>* outputPtr = nullptr;
    const std::atomic<float>* metroOnPtr = nullptr;
    const std::atomic<float>* metroBpmPtr = nullptr;
    const std::atomic<float>* metroBeatsPtr = nullptr;
    const std::atomic<float>* metroNotePtr = nullptr;
    const std::atomic<float>* metroVolPtr = nullptr;

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
