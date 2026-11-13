#include "PluginEditor.h"

namespace
{
    const juce::Colour accent { 0xffe8a33d };
    const juce::Colour inTuneGreen { 0xff4cd964 };
    const juce::Colour muteRed { 0xffe5484d };

    // Base parameter ids and display names, in knob order. The last one (pan) is only used in stereo mode.
    const char* const knobNames[] = { "GAIN", "WARMTH", "BASS", "MID", "TREBLE", "REVERB", "MASTER", "PAN" };

    const char* baseIdFor (int knob)
    {
        return knob < 7 ? GuitarDriveProcessor::knobIds[knob] : GuitarDriveProcessor::panId;
    }

    void styleButton (juce::TextButton& b, juce::Colour onColour)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2c33));
        b.setColour (juce::TextButton::buttonOnColourId, onColour);
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    }
}

// ---------------------------------------------------------------- tuner view

TunerView::TunerView (GuitarDriveProcessor& p)
    : proc (p), frame ((size_t) PitchDetector::frameSize, 0.0f)
{
}

void TunerView::visibilityChanged()
{
    if (isVisible())
    {
        resetReading();
        startTimerHz (30);
    }
    else
    {
        stopTimer();
    }
}

void TunerView::resetReading()
{
    histCount = histPos = 0;
    haveNote = false;
    shownNote = -1000;
    displayCents = cents = 0.0f;
    lastValidMs = 0.0;
}

void TunerView::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    double fs = 0.0;

    if (proc.getTunerFrame (frame.data(), fs))
    {
        const auto result = detector.detect (frame.data(), fs);
        if (result.valid)
        {
            if (now - lastValidMs > 600.0)
                histCount = histPos = 0;

            history[(size_t) histPos] = (float) midiFromFrequency (result.frequency);
            histPos = (histPos + 1) % (int) history.size();
            histCount = std::min (histCount + 1, (int) history.size());
            lastValidMs = now;

            // The median of the last few readings keeps the display from jumping between notes.
            auto sorted = history;
            std::sort (sorted.begin(), sorted.begin() + histCount);
            const float midi = sorted[(size_t) (histCount / 2)];
            note = (int) std::lround (midi);
            cents = (midi - (float) note) * 100.0f;
            hz = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
            haveNote = true;
        }
    }

    if (haveNote && now - lastValidMs > 400.0)
        haveNote = false;

    if (haveNote)
    {
        if (note != shownNote)
        {
            shownNote = note;
            displayCents = cents;
        }
        displayCents += 0.35f * (cents - displayCents);
    }
    else
    {
        shownNote = -1000;
    }

    repaint();
}

void TunerView::paint (juce::Graphics& g)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const auto b = getLocalBounds().toFloat();
    const bool inTune = haveNote && std::abs (displayCents) <= 5.0f;
    const auto noteColour = ! haveNote ? juce::Colours::grey : inTune ? inTuneGreen : accent;

    const juce::String noteText = haveNote ? juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1) : "--";
    g.setColour (noteColour);
    g.setFont (juce::FontOptions (92.0f, juce::Font::bold));
    g.drawText (noteText, b.withHeight (104.0f).toNearestInt(), juce::Justification::centred);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (haveNote ? juce::String (hz, 1) + " Hz" : "Play a string", juce::Rectangle<int> (0, 104, getWidth(), 20),
                juce::Justification::centred);

    // meter: -50..+50 cents
    const float halfWidth = std::min (260.0f, b.getWidth() * 0.5f - 30.0f);
    const float cx = b.getCentreX(), meterY = 150.0f;
    g.setColour (juce::Colour (0xff2c2c33));
    g.fillRoundedRectangle (cx - halfWidth, meterY, halfWidth * 2.0f, 16.0f, 4.0f);

    g.setColour (inTuneGreen.withAlpha (0.35f));
    g.fillRect (cx - halfWidth * 0.1f, meterY, halfWidth * 0.2f, 16.0f);   // +/-5 cents

    g.setFont (juce::FontOptions (12.0f));
    for (int c = -50; c <= 50; c += 10)
    {
        const float x = cx + halfWidth * (float) c / 50.0f;
        const bool major = c == 0 || std::abs (c) == 50;
        g.setColour (juce::Colours::grey);
        g.drawLine (x, meterY + 16.0f, x, meterY + (major ? 28.0f : 22.0f), 1.0f);
        if (major)
            g.drawText (c > 0 ? "+" + juce::String (c) : juce::String (c),
                        juce::Rectangle<int> ((int) x - 25, (int) meterY + 30, 50, 16), juce::Justification::centred);
    }

    if (haveNote)
    {
        const float x = cx + halfWidth * juce::jlimit (-50.0f, 50.0f, displayCents) / 50.0f;
        g.setColour (noteColour);
        g.fillRoundedRectangle (x - 3.0f, meterY - 10.0f, 6.0f, 36.0f, 3.0f);
    }

    // the number
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (44.0f, juce::Font::bold));
    if (haveNote)
    {
        const int c = (int) std::lround (displayCents);
        g.drawText ((c > 0 ? "+" + juce::String (c) : juce::String (c)) + " cents",
                    juce::Rectangle<int> (0, 200, getWidth(), 52), juce::Justification::centred);

        g.setColour (noteColour);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (inTune ? "IN TUNE" : displayCents < 0.0f ? "FLAT - tune up" : "SHARP - tune down",
                    juce::Rectangle<int> (0, 252, getWidth(), 22), juce::Justification::centred);
    }

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Output is muted while the tuner is open", juce::Rectangle<int> (0, getHeight() - 24, getWidth(), 20),
                juce::Justification::centred);
}

// ---------------------------------------------------------------- editor

GuitarDriveEditor::GuitarDriveEditor (GuitarDriveProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      tunerView (p),
      stereoAttach (*p.apvts.getParameter (GuitarDriveProcessor::stereoId), [this] (float v) { setStereo (v > 0.5f); }),
      soloAttach (*p.apvts.getParameter (GuitarDriveProcessor::soloId),
                  [this] (float v) { soloState = (int) std::lround (v); updateSoloButtons(); }),
      rightInAttach (*p.apvts.getParameter (GuitarDriveProcessor::rightSameId),
                     [this] (float v) { rightUsesIn1 = v > 0.5f; rightInButton.setToggleState (rightUsesIn1, juce::dontSendNotification); })
{
    for (int s = 0; s < 2; ++s)
    {
        auto& strip = strips[(size_t) s];

        for (int k = 0; k < numKnobs; ++k)
        {
            auto& knob = strip.knobs[(size_t) k];
            const auto id = GuitarDriveProcessor::paramId (s, baseIdFor (k));
            auto* param = p.apvts.getParameter (id);

            knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, knob.slider);
            knob.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
            knob.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
            knob.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3a40));
            knob.slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
            knob.slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            knob.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addChildComponent (knob.slider);

            knob.label.setText (knobNames[k], juce::dontSendNotification);
            knob.label.setJustificationType (juce::Justification::centred);
            knob.label.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            knob.label.setColour (juce::Label::textColourId, accent);
            addChildComponent (knob.label);
        }

        strip.name.setText (s == 0 ? "LEFT" : "RIGHT", juce::dontSendNotification);
        strip.name.setJustificationType (juce::Justification::centred);
        strip.name.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        strip.name.setColour (juce::Label::textColourId, juce::Colours::white);
        addChildComponent (strip.name);

        styleButton (strip.solo, accent);
        styleButton (strip.mute, muteRed);
        addChildComponent (strip.solo);
        addChildComponent (strip.mute);
    }

    // Solo and mute are linked across the channels: soloing one side is the same as muting the other.
    strips[0].solo.onClick = [this] { setSolo (soloState == 1 ? 0 : 1); };
    strips[1].mute.onClick = [this] { setSolo (soloState == 1 ? 0 : 1); };
    strips[1].solo.onClick = [this] { setSolo (soloState == 2 ? 0 : 2); };
    strips[0].mute.onClick = [this] { setSolo (soloState == 2 ? 0 : 2); };

    styleButton (tunerButton, accent);
    tunerButton.setClickingTogglesState (true);
    tunerButton.onClick = [this] { setTunerOpen (tunerButton.getToggleState()); };
    addAndMakeVisible (tunerButton);

    styleButton (stereoButton, accent);
    stereoButton.onClick = [this] { stereoAttach.setValueAsCompleteGesture (stereoMode ? 0.0f : 1.0f); };
    addAndMakeVisible (stereoButton);

    // On: the right strip hears input 1 (one guitar through two chains). Off: it hears input 2.
    styleButton (rightInButton, accent);
    rightInButton.onClick = [this] { rightInAttach.setValueAsCompleteGesture (rightUsesIn1 ? 0.0f : 1.0f); };
    addChildComponent (rightInButton);

    addChildComponent (tunerView);

    rightInAttach.sendInitialUpdate();
    soloAttach.sendInitialUpdate();
    stereoAttach.sendInitialUpdate();
}

GuitarDriveEditor::~GuitarDriveEditor()
{
    proc.setTunerActive (false);   // never leave the output muted behind a closed window
}

void GuitarDriveEditor::setStereo (bool on)
{
    stereoMode = on;
    stereoButton.setToggleState (on, juce::dontSendNotification);
    updateVisibility();
    setSize (on ? 960 : 780, on ? 500 : 400);
    resized();
    repaint();
}

void GuitarDriveEditor::setTunerOpen (bool open)
{
    tunerOpen = open;
    proc.setTunerActive (open);
    updateVisibility();
    repaint();
}

void GuitarDriveEditor::setSolo (int state)
{
    soloAttach.setValueAsCompleteGesture ((float) state);
}

void GuitarDriveEditor::updateSoloButtons()
{
    strips[0].solo.setToggleState (soloState == 1, juce::dontSendNotification);
    strips[1].mute.setToggleState (soloState == 1, juce::dontSendNotification);
    strips[1].solo.setToggleState (soloState == 2, juce::dontSendNotification);
    strips[0].mute.setToggleState (soloState == 2, juce::dontSendNotification);
}

void GuitarDriveEditor::updateVisibility()
{
    for (int s = 0; s < 2; ++s)
    {
        auto& strip = strips[(size_t) s];
        const bool stripShown = ! tunerOpen && (s == 0 || stereoMode);

        for (int k = 0; k < numKnobs; ++k)
        {
            const bool shown = stripShown && (k < 7 || stereoMode);
            strip.knobs[(size_t) k].slider.setVisible (shown);
            strip.knobs[(size_t) k].label.setVisible (shown);
        }
        strip.name.setVisible (stripShown && stereoMode);
        strip.solo.setVisible (stripShown && stereoMode);
        strip.mute.setVisible (stripShown && stereoMode);
    }
    rightInButton.setVisible (stereoMode && ! tunerOpen);
    tunerView.setVisible (tunerOpen);
}

void GuitarDriveEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c20));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("GuitarDrive", 20, 10, 300, 30, juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    if (! tunerOpen)
        g.drawText (stereoMode ? "Scroll over a knob to turn it  |  Shift = fine  |  Double-click = reset  |  R USES IN 1: on = right strip hears input 1, off = input 2"
                               : "Scroll over a knob to turn it  |  Shift = fine  |  Double-click = reset",
                    20, getHeight() - 28, getWidth() - 40, 20, juce::Justification::centredLeft);

    if (stereoMode && ! tunerOpen)
    {
        g.setColour (juce::Colour (0xff33333a));
        g.drawHorizontalLine (50 + (getHeight() - 80) / 2, 20.0f, (float) getWidth() - 20.0f);
    }
}

void GuitarDriveEditor::layoutStrip (StripUI& strip, juce::Rectangle<int> row, bool stereo)
{
    if (stereo)
    {
        strip.name.setBounds (row.removeFromLeft (56));

        auto buttons = row.removeFromRight (80).reduced (4, 0);
        const int h = 30, gap = 10;
        auto block = buttons.withSizeKeepingCentre (buttons.getWidth(), h * 2 + gap);
        strip.solo.setBounds (block.removeFromTop (h));
        block.removeFromTop (gap);
        strip.mute.setBounds (block);
    }

    const int cols = stereo ? numKnobs : 7;
    const int colWidth = row.getWidth() / cols;
    for (int k = 0; k < cols; ++k)
    {
        auto col = row.removeFromLeft (colWidth);
        auto& knob = strip.knobs[(size_t) k];
        knob.label.setBounds (col.removeFromTop (24));
        knob.slider.setBounds (col.reduced (6));
    }
}

void GuitarDriveEditor::resized()
{
    tunerButton.setBounds (getWidth() - 20 - 90, 12, 90, 28);
    stereoButton.setBounds (getWidth() - 20 - 90 - 10 - 100, 12, 100, 28);
    rightInButton.setBounds (getWidth() - 20 - 90 - 10 - 100 - 10 - 130, 12, 130, 28);

    auto area = getLocalBounds().reduced (20, 0);
    area.removeFromTop (50);
    area.removeFromBottom (30);
    tunerView.setBounds (area);

    if (stereoMode)
    {
        const int rowHeight = area.getHeight() / 2;
        layoutStrip (strips[0], area.removeFromTop (rowHeight), true);
        layoutStrip (strips[1], area, true);
    }
    else
    {
        layoutStrip (strips[0], area, false);
    }
}
