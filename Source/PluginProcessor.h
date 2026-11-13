#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ChannelStrip.h"
#include "IRPack.h"
#include "Effects.h"
#include "Metronome.h"
#include "Params.h"
#include "Presets.h"
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

    // Everything on a channel (amp knobs, pedals, EQ, effects) exists twice: strip 0 is the mono / left channel,
    // strip 1 the right channel, whose parameter ids carry an "r_" prefix.
    static constexpr const char* panId = "pan";            // -1 (left) .. +1 (right), stereo mode only
    static constexpr const char* revTypeId = "revtype";    // 0 = plate (smooth), 1 = spring
    static constexpr const char* ampTypeId = "amptype";    // 0 = Fender (Twin Reverb), 1 = Two-Rock, 2 = both side by side
    static constexpr const char* stereoId = "stereo";      // false = mono (default), true = two channels
    static constexpr const char* soloId = "solo";          // 0 = off, 1 = left soloed (right muted), 2 = right soloed (left muted)
    static constexpr const char* rightSameId = "rightsame";   // stereo mode: true = right channel hears input 1 too, false = input 2

    static juce::String paramId (int strip, const char* base) { return strip == 0 ? juce::String (base) : "r_" + juce::String (base); }

    // Tuner: while active the output is muted and the input is analysed.
    void setTunerActive (bool on) { tunerActive.store (on); }
    bool isTunerActive() const { return tunerActive.load(); }
    // Copies the latest PitchDetector::frameSize decimated samples; false until enough have arrived.
    bool getTunerFrame (float* dest, double& sampleRate) const;

    const Metronome& getMetronome() const { return metronome; }

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
    ChannelStrip::Params paramsFor (int strip) const;
    void feedTuner (const float* mono, int n);
    void ensureScratch (int n);

    // scratch channels
    enum { in0, in1, out0L, out0R, out1L, out1R, mixL, mixR, numScratch };

    ChannelStrip strips[2];

    // recorded cabinets and spring tank (only in builds that embed them), kept up to date with the MIC, DWELL and
    // TONE knobs by a background thread
    struct IRUpdater : juce::Thread
    {
        explicit IRUpdater (GuitarDriveProcessor& p);
        ~IRUpdater() override;
        void run() override;
        GuitarDriveProcessor& owner;
    };
    std::unique_ptr<IRPack> irPack;
    std::unique_ptr<IRUpdater> irUpdater;   // declared after strips and irPack, so it stops before they go
    Metronome metronome;
    juce::AudioBuffer<float> scratch { numScratch, 0 };

    juce::SmoothedValue<float> inputGain, outputGain, muteSmooth, switchFade;
    juce::SmoothedValue<float> chanGain[2], chanCenter[2], chanPan[2];
    bool wasStereo = false;

    // cached parameter pointers
    const std::atomic<float>* stereoPtr = nullptr;
    const std::atomic<float>* soloPtr = nullptr;
    const std::atomic<float>* rightSamePtr = nullptr;
    const std::atomic<float>* inputPtr = nullptr;
    const std::atomic<float>* outputPtr = nullptr;
    const std::atomic<float>* panPtr[2] = {};
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
