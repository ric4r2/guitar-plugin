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

class GuitarDriveEditor : public juce::AudioProcessorEditor
{
public:
    explicit GuitarDriveEditor (GuitarDriveProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        ScrollKnob slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::array<Knob, 6> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDriveEditor)
};
