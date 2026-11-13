#pragma once
#include "PluginProcessor.h"

// Rotary knob driven by the mouse wheel: each notch moves 3% of the range, Shift makes it 0.5%.
class ScrollKnob : public juce::Slider
{
public:
    ScrollKnob()
        : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow)
    {
        setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                             juce::MathConstants<float>::pi * 2.75f, true);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const float delta = (wheel.isReversed ? -1.0f : 1.0f) * wheel.deltaY;
        if (delta == 0.0f)
            return;

        const double step = e.mods.isShiftDown() ? 0.005 : 0.03;
        const double prop = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + (delta > 0 ? step : -step));
        setValue (proportionOfLengthToValue (prop), juce::sendNotificationSync);
    }
};

// Chromatic tuner shown in place of the knobs: note name, cents offset and a meter.
class TunerView : public juce::Component, private juce::Timer
{
public:
    explicit TunerView (GuitarDriveProcessor&);

    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void resetReading();

    GuitarDriveProcessor& proc;
    PitchDetector detector;
    std::vector<float> frame;

    std::array<float, 5> history {};
    int histCount = 0, histPos = 0;
    double lastValidMs = 0.0;

    bool haveNote = false;
    int note = 0, shownNote = -1000;
    float cents = 0.0f, displayCents = 0.0f;
    double hz = 0.0;
};

class GuitarDriveEditor : public juce::AudioProcessorEditor
{
public:
    explicit GuitarDriveEditor (GuitarDriveProcessor&);
    ~GuitarDriveEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int numKnobs = 8;   // gain, warmth, bass, mid, treble, reverb, master, pan

    struct Knob
    {
        ScrollKnob slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct StripUI
    {
        std::array<Knob, numKnobs> knobs;
        juce::Label name;
        juce::TextButton solo { "SOLO" }, mute { "MUTE" };
    };

    void layoutStrip (StripUI&, juce::Rectangle<int> row, bool stereo);
    void setStereo (bool);
    void setTunerOpen (bool);
    void setSolo (int state);
    void updateSoloButtons();
    void updateVisibility();

    GuitarDriveProcessor& proc;
    std::array<StripUI, 2> strips;
    juce::TextButton tunerButton { "TUNER" }, stereoButton { "STEREO" };
    TunerView tunerView;
    juce::ParameterAttachment stereoAttach, soloAttach;

    bool stereoMode = false, tunerOpen = false;
    int soloState = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDriveEditor)
};
