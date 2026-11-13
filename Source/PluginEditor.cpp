#include "PluginEditor.h"

namespace
{
    struct KnobSpec { const char* id; const char* name; };
    const KnobSpec specs[] = {
        { GuitarDriveProcessor::gainId,   "GAIN"   },
        { GuitarDriveProcessor::bassId,   "BASS"   },
        { GuitarDriveProcessor::midId,    "MID"    },
        { GuitarDriveProcessor::trebleId, "TREBLE" },
        { GuitarDriveProcessor::reverbId, "REVERB" },
        { GuitarDriveProcessor::masterId, "MASTER" },
    };

    const juce::Colour accent { 0xffe8a33d };
}

GuitarDriveEditor::GuitarDriveEditor (GuitarDriveProcessor& p)
    : juce::AudioProcessorEditor (&p)
{
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        auto* param = p.apvts.getParameter (specs[i].id);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, specs[i].id, k.slider);
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        k.slider.setNumDecimalPlacesToDisplay (1);
        k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        k.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3a40));
        k.slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        k.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (k.slider);

        k.label.setText (specs[i].name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        k.label.setColour (juce::Label::textColourId, accent);
        addAndMakeVisible (k.label);
    }

    setSize (660, 300);
}

void GuitarDriveEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c20));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("GuitarDrive", 20, 10, 300, 30, juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Scroll over a knob to turn it  |  Shift = fine  |  Double-click = reset",
                20, getHeight() - 28, getWidth() - 40, 20, juce::Justification::centredLeft);
}

void GuitarDriveEditor::resized()
{
    auto area = getLocalBounds().reduced (20, 0);
    area.removeFromTop (50);
    area.removeFromBottom (30);

    const int colWidth = area.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto col = area.removeFromLeft (colWidth);
        k.label.setBounds (col.removeFromTop (24));
        k.slider.setBounds (col.reduced (6));
    }
}
