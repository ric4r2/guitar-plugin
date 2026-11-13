#include "PluginEditor.h"

#include <cstring>
#include <functional>
#include <string>

namespace
{
    void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f1f3), c.x - r * 0.4f, c.y - r * 0.5f,
                                                 juce::Colour (0xff6b6f76), c.x + r * 0.6f, c.y + r * 0.7f, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xff2c2e33));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.8f);
        g.drawLine (c.x - r * 0.65f, c.y + r * 0.3f, c.x + r * 0.65f, c.y - r * 0.3f, 1.4f);
    }

    void styleAmpButton (juce::TextButton& b, juce::Colour onColour, juce::Colour onText)
    {
        b.setColour (juce::TextButton::buttonColourId, Palette::plateLight);
        b.setColour (juce::TextButton::buttonOnColourId, onColour);
        b.setColour (juce::TextButton::textColourOffId, Palette::cream);
        b.setColour (juce::TextButton::textColourOnId, onText);
        b.setColour (juce::ComboBox::outlineColourId, Palette::gold);
    }

    void panelPlate (juce::Graphics& g, juce::Rectangle<int> r, float corner)
    {
        const auto f = r.toFloat();
        g.setGradientFill (juce::ColourGradient (Palette::graphiteMid, f.getX(), f.getY(), Palette::graphite, f.getX(), f.getBottom(), false));
        g.fillRoundedRectangle (f, corner);
        g.setColour (Palette::silver.withAlpha (0.55f));
        g.drawRoundedRectangle (f.reduced (1.0f), corner, 1.5f);
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
    const auto noteColour = ! haveNote ? Palette::dim : inTune ? Palette::green : Palette::amber;

    const juce::String noteText = haveNote ? juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1) : "--";
    g.setColour (noteColour);
    g.setFont (juce::FontOptions (96.0f, juce::Font::bold));
    g.drawText (noteText, b.withTrimmedTop (20.0f).withHeight (110.0f).toNearestInt(), juce::Justification::centred);

    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText (haveNote ? juce::String (hz, 1) + " Hz" : "Play a string", juce::Rectangle<int> (0, 132, getWidth(), 22),
                juce::Justification::centred);

    // meter: -50..+50 cents
    const float halfWidth = std::min (300.0f, b.getWidth() * 0.5f - 30.0f);
    const float cx = b.getCentreX(), meterY = 180.0f;
    g.setColour (Palette::inkDark);
    g.fillRoundedRectangle (cx - halfWidth, meterY, halfWidth * 2.0f, 18.0f, 4.0f);

    g.setColour (Palette::green.withAlpha (0.35f));
    g.fillRect (cx - halfWidth * 0.1f, meterY, halfWidth * 0.2f, 18.0f);   // +/-5 cents

    g.setFont (juce::FontOptions (12.0f));
    for (int c = -50; c <= 50; c += 10)
    {
        const float x = cx + halfWidth * (float) c / 50.0f;
        const bool major = c == 0 || std::abs (c) == 50;
        g.setColour (Palette::dim);
        g.drawLine (x, meterY + 18.0f, x, meterY + (major ? 30.0f : 24.0f), 1.0f);
        if (major)
            g.drawText (c > 0 ? "+" + juce::String (c) : juce::String (c),
                        juce::Rectangle<int> ((int) x - 25, (int) meterY + 32, 50, 16), juce::Justification::centred);
    }

    if (haveNote)
    {
        const float x = cx + halfWidth * juce::jlimit (-50.0f, 50.0f, displayCents) / 50.0f;
        g.setColour (noteColour);
        g.fillRoundedRectangle (x - 3.0f, meterY - 10.0f, 6.0f, 38.0f, 3.0f);
    }

    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
    if (haveNote)
    {
        const int c = (int) std::lround (displayCents);
        g.drawText ((c > 0 ? "+" + juce::String (c) : juce::String (c)) + " cents",
                    juce::Rectangle<int> (0, 232, getWidth(), 56), juce::Justification::centred);

        g.setColour (noteColour);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText (inTune ? "IN TUNE" : displayCents < 0.0f ? "FLAT - tune up" : "SHARP - tune down",
                    juce::Rectangle<int> (0, 290, getWidth(), 24), juce::Justification::centred);
    }

    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Output is muted while the tuner is open", juce::Rectangle<int> (0, getHeight() - 28, getWidth(), 20),
                juce::Justification::centred);
}

// ---------------------------------------------------------------- amp view

AmpView::AmpView (GuitarDriveProcessor& p)
    : apvts (p.apvts),
      soloAttach (*p.apvts.getParameter (GuitarDriveProcessor::soloId),
                  [this] (float v) { soloState = (int) std::lround (v); updateSoloButtons(); })
{
    const auto& defs = Params::stripKnobs();
    const auto theme = Themes::amp();

    for (int s = 0; s < 2; ++s)
    {
        auto& strip = strips[(size_t) s];

        for (size_t k = 0; k < defs.size(); ++k)
            strip.knobs[k].init (*this, apvts, GuitarDriveProcessor::paramId (s, defs[k].id), defs[k].label, theme);
        strip.knobs[defs.size()].init (*this, apvts, GuitarDriveProcessor::paramId (s, GuitarDriveProcessor::panId), "PAN", theme);

        // transpose moves in whole semitones: one wheel notch is one semitone
        strip.knobs[1].slider.setWheelFractions (1.0 / 24.0, 1.0 / 24.0);

        strip.name.setText (s == 0 ? "LEFT" : "RIGHT", juce::dontSendNotification);
        strip.name.setJustificationType (juce::Justification::centred);
        strip.name.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        strip.name.setColour (juce::Label::textColourId, Palette::cream);
        addAndMakeVisible (strip.name);

        styleAmpButton (strip.solo, Palette::amber, Palette::inkDark);
        styleAmpButton (strip.mute, Palette::red, Palette::cream);
        styleAmpButton (strip.revType, Palette::amber, Palette::inkDark);
        addAndMakeVisible (strip.solo);
        addAndMakeVisible (strip.mute);
        addAndMakeVisible (strip.revType);

        revAttach[(size_t) s] = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (GuitarDriveProcessor::paramId (s, GuitarDriveProcessor::revTypeId)),
            [this, s] (float v)
            {
                auto& st = strips[(size_t) s];
                st.revState = v > 0.5f ? 1 : 0;
                st.revType.setButtonText (st.revState != 0 ? "SPRING" : "PLATE");
                st.revType.setToggleState (st.revState != 0, juce::dontSendNotification);
            });
        strip.revType.onClick = [this, s] { revAttach[(size_t) s]->setValueAsCompleteGesture (strips[(size_t) s].revState != 0 ? 0.0f : 1.0f); };
    }

    // Solo and mute are linked across the channels: soloing one side is the same as muting the other.
    strips[0].solo.onClick = [this] { setSolo (soloState == 1 ? 0 : 1); };
    strips[1].mute.onClick = [this] { setSolo (soloState == 1 ? 0 : 1); };
    strips[1].solo.onClick = [this] { setSolo (soloState == 2 ? 0 : 2); };
    strips[0].mute.onClick = [this] { setSolo (soloState == 2 ? 0 : 2); };

    soloAttach.sendInitialUpdate();
    for (auto& a : revAttach)
        a->sendInitialUpdate();
    setStereo (false);
}

void AmpView::setStereo (bool on)
{
    stereo = on;
    updateVisibility();
    resized();
    repaint();
}

void AmpView::setSolo (int state)
{
    soloAttach.setValueAsCompleteGesture ((float) state);
}

void AmpView::updateSoloButtons()
{
    strips[0].solo.setToggleState (soloState == 1, juce::dontSendNotification);
    strips[1].mute.setToggleState (soloState == 1, juce::dontSendNotification);
    strips[1].solo.setToggleState (soloState == 2, juce::dontSendNotification);
    strips[0].mute.setToggleState (soloState == 2, juce::dontSendNotification);
}

void AmpView::updateVisibility()
{
    for (int s = 0; s < 2; ++s)
    {
        auto& strip = strips[(size_t) s];
        const bool shown = s == 0 || stereo;

        for (int k = 0; k < numKnobs; ++k)
            strip.knobs[(size_t) k].show (shown && (k < numKnobs - 1 || stereo));

        strip.name.setVisible (shown && stereo);
        strip.solo.setVisible (shown && stereo);
        strip.mute.setVisible (shown && stereo);
        strip.revType.setVisible (shown);
    }
}

void AmpView::layoutAmp (int index, juce::Rectangle<int> cabinetArea, bool stereoRow)
{
    auto& strip = strips[(size_t) index];
    const auto i = (size_t) index;

    cabinet[i] = cabinetArea;
    auto inner = cabinetArea.reduced (24, stereoRow ? 14 : 20);
    if (stereoRow)
    {
        plate[i] = inner;
        grille[i] = {};
    }
    else
    {
        plate[i] = inner.removeFromTop (190);
        inner.removeFromTop (14);
        grille[i] = inner;
    }

    auto content = plate[i].reduced (12, 10);
    auto left = content.removeFromLeft (74);
    strip.name.setBounds (left.removeFromBottom (26));

    auto buttons = content.removeFromRight (108).reduced (6, 0);
    const int h = 32, space = 8;
    if (stereoRow)
    {
        auto block = buttons.withSizeKeepingCentre (buttons.getWidth(), h * 3 + space * 2);
        strip.solo.setBounds (block.removeFromTop (h));
        block.removeFromTop (space);
        strip.mute.setBounds (block.removeFromTop (h));
        block.removeFromTop (space);
        strip.revType.setBounds (block);
    }
    else
    {
        strip.revType.setBounds (buttons.withSizeKeepingCentre (buttons.getWidth(), h));
    }

    const int cols = stereoRow ? numKnobs : numKnobs - 1;
    const int colWidth = content.getWidth() / cols;
    for (int k = 0; k < cols; ++k)
        strip.knobs[(size_t) k].place (content.removeFromLeft (colWidth));
}

void AmpView::resized()
{
    auto area = getLocalBounds();
    if (stereo)
    {
        const int rowHeight = area.getHeight() / 2;
        layoutAmp (0, area.removeFromTop (rowHeight), true);
        layoutAmp (1, area, true);
    }
    else
    {
        layoutAmp (0, area, false);
    }
}

void AmpView::paint (juce::Graphics& g)
{
    if (tweedImage.isNull() || tweedImage.getWidth() != getWidth() || tweedImage.getHeight() != getHeight())
        tweedImage = Textures::tweed (getWidth(), getHeight());

    for (int s = 0; s < (stereo ? 2 : 1); ++s)
        paintAmp (g, s);
}

void AmpView::paintAmp (juce::Graphics& g, int index)
{
    const auto i = (size_t) index;
    const auto cab = cabinet[i].toFloat();

    // tweed covering with a dark edge and a cream piping line
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (cab, 16.0f);
        g.reduceClipRegion (clip);
        g.drawImageAt (tweedImage, 0, 0);
    }
    g.setColour (juce::Colour (0xff2a170a));
    g.drawRoundedRectangle (cab.reduced (1.0f), 16.0f, 3.0f);
    g.setColour (Palette::cream.withAlpha (0.5f));
    g.drawRoundedRectangle (cab.reduced (8.0f), 12.0f, 1.2f);

    // control plate: dark brushed metal with a gold border
    const auto pl = plate[i].toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (pl.translated (0.0f, 3.0f), 8.0f);
    g.setGradientFill (juce::ColourGradient (Palette::plateLight, pl.getX(), pl.getY(), Palette::plate, pl.getX(), pl.getBottom(), false));
    g.fillRoundedRectangle (pl, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    for (float y = pl.getY() + 3.0f; y < pl.getBottom() - 3.0f; y += 3.0f)
        g.drawHorizontalLine ((int) y, pl.getX() + 5.0f, pl.getRight() - 5.0f);
    g.setColour (Palette::gold);
    g.drawRoundedRectangle (pl.reduced (1.0f), 8.0f, 2.0f);
    g.setColour (Palette::gold.withAlpha (0.4f));
    g.drawRoundedRectangle (pl.reduced (6.0f), 5.0f, 1.0f);

    drawScrew (g, { pl.getX() + 14.0f, pl.getY() + 14.0f }, 4.5f);
    drawScrew (g, { pl.getRight() - 14.0f, pl.getY() + 14.0f }, 4.5f);
    drawScrew (g, { pl.getX() + 14.0f, pl.getBottom() - 14.0f }, 4.5f);
    drawScrew (g, { pl.getRight() - 14.0f, pl.getBottom() - 14.0f }, 4.5f);

    // red pilot lamp
    const juce::Point<float> lamp (pl.getX() + 12.0f + 37.0f, pl.getY() + 44.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff3b2a).withAlpha (0.5f), lamp.x, lamp.y,
                                             juce::Colours::transparentBlack, lamp.x + 30.0f, lamp.y, true));
    g.fillEllipse (lamp.x - 30.0f, lamp.y - 30.0f, 60.0f, 60.0f);
    g.setColour (juce::Colour (0xff2c2e33));
    g.fillEllipse (lamp.x - 14.0f, lamp.y - 14.0f, 28.0f, 28.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff8a70), lamp.x - 4.0f, lamp.y - 5.0f,
                                             juce::Colour (0xffc01808), lamp.x + 6.0f, lamp.y + 7.0f, true));
    g.fillEllipse (lamp.x - 10.5f, lamp.y - 10.5f, 21.0f, 21.0f);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillEllipse (lamp.x - 6.0f, lamp.y - 7.0f, 7.0f, 4.5f);
    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("PILOT", juce::Rectangle<float> (lamp.x - 30.0f, lamp.y + 18.0f, 60.0f, 14.0f).toNearestInt(), juce::Justification::centred);

    // speaker grille cloth with a badge (mono layout only)
    if (! grille[i].isEmpty())
    {
        const auto gr = grille[i].toFloat();
        if (grilleImage.isNull() || grilleImage.getWidth() != grille[i].getWidth() || grilleImage.getHeight() != grille[i].getHeight())
            grilleImage = Textures::grille (grille[i].getWidth(), grille[i].getHeight());

        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (gr, 6.0f);
            g.reduceClipRegion (clip);
            g.drawImageAt (grilleImage, grille[i].getX(), grille[i].getY());
        }
        g.setColour (juce::Colour (0xff3b2412));
        g.drawRoundedRectangle (gr, 6.0f, 4.0f);
        g.setColour (Palette::gold.withAlpha (0.6f));
        g.drawRoundedRectangle (gr.reduced (4.0f), 4.0f, 1.0f);

        const auto logo = juce::Rectangle<float> (230.0f, 42.0f).withCentre (gr.getCentre());
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (logo.translated (0.0f, 2.0f), 6.0f);
        g.setColour (Palette::plate);
        g.fillRoundedRectangle (logo, 6.0f);
        g.setColour (Palette::gold);
        g.drawRoundedRectangle (logo.reduced (1.0f), 6.0f, 1.6f);
        g.setColour (Palette::cream);
        g.setFont (juce::FontOptions (25.0f, juce::Font::bold | juce::Font::italic));
        g.drawText ("GuitarDrive", logo.toNearestInt(), juce::Justification::centred);
    }
}

// ---------------------------------------------------------------- pedals

PedalPanel::Colours PedalPanel::coloursFor (const juce::String& onId)
{
    const Colours purple { juce::Colour (0xffd5c3e6), juce::Colour (0xffb56bff) };
    const Colours blue   { juce::Colour (0xffbcd8ec), juce::Colour (0xff42a9ff) };
    const Colours green  { juce::Colour (0xffc5e0ba), juce::Colour (0xff3dff6c) };
    const Colours red    { juce::Colour (0xffe8ab9d), juce::Colour (0xffff3b30) };
    const Colours pink   { juce::Colour (0xffecc0c9), juce::Colour (0xffff5fa8) };

    if (onId == "wah_on") return purple;
    if (onId == "comp_on") return blue;
    if (onId == "ts_on") return green;
    if (onId == "od_on") return red;
    if (onId == "ph_on") return pink;
    if (onId == "ch_on") return blue;
    if (onId == "tr_on") return purple;
    if (onId == "dl_on") return green;
    if (onId == "rv_on") return red;
    return blue;
}

PedalPanel::PedalPanel (juce::AudioProcessorValueTreeState& apvts, const Params::PedalDef& d)
    : def (d), colours (coloursFor (d.onId)), footSwitch (colours.led)
{
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, def.onId, footSwitch);
    addAndMakeVisible (footSwitch);

    if (def.toggleId != nullptr)
    {
        toggleButton.setButtonText (def.toggleLabel);
        toggleButton.setClickingTogglesState (true);
        toggleButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff4a4d53));
        toggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, def.toggleId, toggleButton);
        addAndMakeVisible (toggleButton);
    }

    const auto theme = Themes::pedal (colours.knob);
    for (const auto& k : def.knobs)
    {
        knobs.push_back (std::make_unique<KnobUI>());
        knobs.back()->init (*this, apvts, k.id, k.label, theme);
    }
}

void PedalPanel::paint (juce::Graphics& g)
{
    if (brushed.isNull() || brushed.getWidth() != getWidth() || brushed.getHeight() != getHeight())
        brushed = Textures::brushedAluminum (getWidth(), getHeight(), (int) std::hash<std::string> {} (def.title) & 0xffff);

    const auto b = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (b.translated (0.0f, 4.0f), 12.0f);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (b, 12.0f);
        g.reduceClipRegion (clip);
        g.drawImageAt (brushed, 0, 0);
    }
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (b.reduced (1.5f), 11.0f, 1.2f);
    g.setColour (juce::Colour (0xff383b40));
    g.drawRoundedRectangle (b, 12.0f, 1.6f);

    drawScrew (g, { b.getX() + 11.0f, b.getY() + 11.0f }, 3.6f);
    drawScrew (g, { b.getRight() - 11.0f, b.getY() + 11.0f }, 3.6f);
    drawScrew (g, { b.getX() + 11.0f, b.getBottom() - 11.0f }, 3.6f);
    drawScrew (g, { b.getRight() - 11.0f, b.getBottom() - 11.0f }, 3.6f);

    // silkscreened name with a stripe in the pedal's color
    g.setColour (Palette::inkDark);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText (def.title, titleArea, juce::Justification::centredLeft);
    g.setColour (colours.led);
    g.fillRoundedRectangle ((float) titleArea.getX(), (float) titleArea.getBottom() - 1.0f, 46.0f, 4.0f, 2.0f);
}

void PedalPanel::resized()
{
    auto area = getLocalBounds().reduced (20, 14);
    footSwitch.setBounds (area.removeFromRight (84));
    area.removeFromRight (6);

    auto header = area.removeFromTop (28);
    if (def.toggleId != nullptr)
        toggleButton.setBounds (header.removeFromRight (62).reduced (0, 1));
    titleArea = header;

    area.removeFromTop (6);
    const int colWidth = area.getWidth() / juce::jmax (1, (int) knobs.size());
    for (auto& k : knobs)
        k->place (area.removeFromLeft (colWidth), 18);
}

PedalBoard::PedalBoard (juce::AudioProcessorValueTreeState& apvts, const std::vector<Params::PedalDef>& defs,
                        std::vector<std::vector<int>> rowLayout)
    : rows (std::move (rowLayout))
{
    for (const auto& d : defs)
    {
        panels.push_back (std::make_unique<PedalPanel> (apvts, d));
        addAndMakeVisible (*panels.back());
    }
}

void PedalBoard::resized()
{
    auto area = getLocalBounds().reduced (8, 6);
    const int rowHeight = area.getHeight() / juce::jmax (1, (int) rows.size());

    for (const auto& row : rows)
    {
        auto rowArea = area.removeFromTop (rowHeight).reduced (0, 4);

        float total = 0.0f;
        for (int idx : row)
            total += (float) panels[(size_t) idx]->getNumKnobs() + 1.1f;

        const int rowWidth = rowArea.getWidth();   // measured before panels are cut off the left edge
        for (int idx : row)
        {
            auto& panel = *panels[(size_t) idx];
            const int w = (int) ((float) rowWidth * ((float) panel.getNumKnobs() + 1.1f) / total);
            panel.setBounds (rowArea.removeFromLeft (w).reduced (5, 0));
        }
    }
}

// ---------------------------------------------------------------- equalizer

EqView::EqView (juce::AudioProcessorValueTreeState& apvts)
{
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, Params::eqOn, footSwitch);
    addAndMakeVisible (footSwitch);

    const juce::Colour cap (0xffbcd8ec);   // creamy blue fader caps
    for (int b = 0; b < Params::numEqBands; ++b)
    {
        auto& band = bands[(size_t) b];
        band.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Params::eqId (b), band.fader);
        band.fader.setDoubleClickReturnValue (true, 0.0);
        band.fader.onValueChange = [this] { updateCurve(); };
        band.fader.setColour (juce::Slider::thumbColourId, cap);
        band.fader.setColour (juce::Slider::backgroundColourId, Palette::inkDark);
        band.fader.setColour (juce::Slider::textBoxTextColourId, Palette::inkDark);
        addAndMakeVisible (band.fader);

        band.label.setText (juce::String (Params::eqLabel (b)) + " Hz", juce::dontSendNotification);
        band.label.setJustificationType (juce::Justification::centred);
        band.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        band.label.setColour (juce::Label::textColourId, Palette::inkDark);
        addAndMakeVisible (band.label);
    }
    updateCurve();
}

void EqView::updateCurve()
{
    for (int b = 0; b < Params::numEqBands; ++b)
        coefs[(size_t) b] = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            48000.0, Params::eqFreqs[b], 1.0f, juce::Decibels::decibelsToGain ((float) bands[(size_t) b].fader.getValue()));
    repaint (curveArea);
}

void EqView::resized()
{
    auto area = getLocalBounds().reduced (22, 18);
    footSwitch.setBounds (area.removeFromRight (88).removeFromTop (118));
    area.removeFromRight (10);

    titleArea = area.removeFromTop (26);
    area.removeFromTop (4);
    curveArea = area.removeFromTop (juce::jmin (118, area.getHeight() / 3));
    area.removeFromTop (8);

    auto faders = getLocalBounds().reduced (22, 18).withTop (area.getY());
    const int colWidth = faders.getWidth() / Params::numEqBands;
    for (auto& band : bands)
    {
        auto col = faders.removeFromLeft (colWidth);
        band.label.setBounds (col.removeFromTop (22));
        band.fader.setBounds (col.reduced (6, 0));
    }
    updateCurve();
}

void EqView::paint (juce::Graphics& g)
{
    if (brushed.isNull() || brushed.getWidth() != getWidth() || brushed.getHeight() != getHeight())
        brushed = Textures::brushedAluminum (getWidth(), getHeight(), 99);

    const auto b = getLocalBounds().toFloat().reduced (6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (b.translated (0.0f, 4.0f), 12.0f);
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (b, 12.0f);
        g.reduceClipRegion (clip);
        g.drawImageAt (brushed, 0, 0);
    }
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (b.reduced (1.5f), 11.0f, 1.2f);
    g.setColour (juce::Colour (0xff383b40));
    g.drawRoundedRectangle (b, 12.0f, 1.6f);
    drawScrew (g, { b.getX() + 12.0f, b.getY() + 12.0f }, 3.8f);
    drawScrew (g, { b.getRight() - 12.0f, b.getY() + 12.0f }, 3.8f);
    drawScrew (g, { b.getX() + 12.0f, b.getBottom() - 12.0f }, 3.8f);
    drawScrew (g, { b.getRight() - 12.0f, b.getBottom() - 12.0f }, 3.8f);

    g.setColour (Palette::inkDark);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText ("GRAPHIC EQUALIZER", titleArea, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff42a9ff));
    g.fillRoundedRectangle ((float) titleArea.getX(), (float) titleArea.getBottom() - 1.0f, 46.0f, 4.0f, 2.0f);

    // response display
    const auto r = curveArea.toFloat();
    g.setColour (juce::Colour (0xff0d0e10));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Palette::silver.withAlpha (0.4f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    const float centreY = r.getCentreY(), halfH = r.getHeight() * 0.40f;
    const auto xForFreq = [&] (float f) { return r.getX() + std::log (f / 30.0f) / std::log (16000.0f / 30.0f) * r.getWidth(); };

    g.setColour (Palette::dim.withAlpha (0.3f));
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
        g.drawHorizontalLine ((int) (centreY - db / 12.0f * halfH), r.getX(), r.getRight());
    for (int i = 0; i < Params::numEqBands; ++i)
        g.drawVerticalLine ((int) xForFreq (Params::eqFreqs[i]), r.getY(), r.getBottom());

    juce::Path path;
    constexpr int points = 160;
    for (int i = 0; i < points; ++i)
    {
        const float f = 30.0f * std::pow (16000.0f / 30.0f, (float) i / (float) (points - 1));
        float db = 0.0f;
        for (const auto& c : coefs)
            if (c != nullptr)
                db += juce::Decibels::gainToDecibels ((float) c->getMagnitudeForFrequency (f, 48000.0));

        const float x = r.getX() + (float) i / (float) (points - 1) * r.getWidth();
        const float y = centreY - juce::jlimit (-15.0f, 15.0f, db) / 12.0f * halfH;
        if (i == 0)
            path.startNewSubPath (x, y);
        else
            path.lineTo (x, y);
    }
    g.setColour (Palette::amber);
    g.strokePath (path, juce::PathStrokeType (2.2f));
}

// ---------------------------------------------------------------- metronome

MetronomeView::MetronomeView (GuitarDriveProcessor& p)
    : proc (p),
      sigAttach (*p.apvts.getParameter (Params::metroSig),
                 [this] (float v)
                 {
                     sigIndex = (int) std::lround (v);
                     for (size_t i = 0; i < sigButtons.size(); ++i)
                         sigButtons[i].setToggleState ((int) i == sigIndex, juce::dontSendNotification);
                     repaint (displayArea);
                 })
{
    const auto theme = Themes::neutral();
    bpm.init (*this, p.apvts, Params::metroBpm, "BPM", theme);
    bpm.slider.setWheelFractions (1.0 / 230.0, 1.0 / 230.0);   // one beat per minute per wheel notch
    bpm.slider.onValueChange = [this] { repaint (displayArea); };
    vol.init (*this, p.apvts, Params::metroVol, "VOLUME", theme);

    runAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Params::metroOn, runSwitch);
    addAndMakeVisible (runSwitch);

    const auto caption = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, Palette::dim);
        addAndMakeVisible (l);
    };
    caption (sigLabel, "TIME SIGNATURE");
    caption (runLabel, "START / STOP");
    caption (tapLabel, "TAP TEMPO");

    tapButton.onClick = [this] { tap(); };
    addAndMakeVisible (tapButton);

    for (size_t i = 0; i < sigButtons.size(); ++i)
    {
        auto& b = sigButtons[i];
        b.setButtonText (Metronome::sigName ((int) i));
        b.setRadioGroupId (202);
        b.onClick = [this, i] { sigAttach.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    sigAttach.sendInitialUpdate();
}

void MetronomeView::visibilityChanged()
{
    if (isVisible())
        startTimerHz (25);
    else
        stopTimer();
}

void MetronomeView::tap()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.empty() && now - taps.back() > 2000.0)
        taps.clear();

    taps.push_back (now);
    if (taps.size() > 6)
        taps.erase (taps.begin());

    if (taps.size() >= 2)
    {
        const double averageMs = (taps.back() - taps.front()) / (double) (taps.size() - 1);
        bpm.slider.setValue (juce::jlimit (30.0, 260.0, 60000.0 / averageMs), juce::sendNotificationSync);
    }
}

void MetronomeView::resized()
{
    // the controls take 290 px; centre them vertically in whatever room there is
    auto area = getLocalBounds().reduced (24, 0).withSizeKeepingCentre (getWidth() - 48, juce::jmin (290, getHeight() - 24));

    auto top = area.removeFromTop (200);
    bpm.place (top.removeFromLeft (170), 22);
    top.removeFromLeft (16);
    displayArea = top.removeFromLeft (420).withTrimmedBottom (8);
    top.removeFromLeft (24);

    auto tapColumn = top.removeFromLeft (150);
    tapLabel.setBounds (tapColumn.removeFromBottom (24));
    tapButton.setBounds (tapColumn.withSizeKeepingCentre (150, 100));
    top.removeFromLeft (14);

    auto runColumn = top.removeFromLeft (100);
    runLabel.setBounds (runColumn.removeFromBottom (24));
    runSwitch.setBounds (runColumn);
    top.removeFromLeft (14);

    vol.place (top, 22);

    area.removeFromTop (14);
    sigLabel.setBounds (area.removeFromTop (22));
    auto row = area.removeFromTop (54);
    const int w = row.getWidth() / Metronome::numSigs;
    for (auto& b : sigButtons)
        b.setBounds (row.removeFromLeft (w).reduced (6, 0));
}

void MetronomeView::paint (juce::Graphics& g)
{
    const auto r = displayArea.toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0e10), r.getX(), r.getY(), juce::Colour (0xff17181b), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Palette::silver.withAlpha (0.45f));
    g.drawRoundedRectangle (r.reduced (1.0f), 10.0f, 1.5f);

    const int bpmValue = (int) std::lround (proc.apvts.getRawParameterValue (Params::metroBpm)->load());
    g.setColour (Palette::amber);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 88.0f, juce::Font::bold));
    g.drawText (juce::String (bpmValue), r.withTrimmedBottom (64.0f).withTrimmedLeft (16.0f).toNearestInt(), juce::Justification::centredLeft);

    const int sig = sigIndex;
    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText ("BPM", r.withTrimmedTop (18.0f).withTrimmedRight (20.0f).withHeight (24.0f).toNearestInt(), juce::Justification::centredRight);
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText (Metronome::sigName (sig), r.withTrimmedTop (44.0f).withTrimmedRight (20.0f).withHeight (36.0f).toNearestInt(), juce::Justification::centredRight);
    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (11.0f));
    const bool eighths = std::strstr (Metronome::sigName (sig), "/8") != nullptr;
    g.drawText (eighths ? "BPM counts eighth notes" : "BPM counts quarter notes",
                r.withTrimmedTop (80.0f).withTrimmedRight (20.0f).withHeight (16.0f).toNearestInt(), juce::Justification::centredRight);

    // pulse lights
    const int pulses = Metronome::numPulses (sig);
    const int current = proc.getMetronome().getPulse();
    constexpr float d = 26.0f, space = 10.0f;
    const float total = (float) pulses * d + (float) (pulses - 1) * space;
    const float startX = r.getX() + 16.0f;
    const float y = r.getBottom() - 22.0f - d * 0.5f;
    (void) total;

    for (int i = 0; i < pulses; ++i)
    {
        const auto dot = juce::Rectangle<float> (d, d).withCentre ({ startX + d * 0.5f + (float) i * (d + space), y });
        const int accent = Metronome::accentFor (sig, i);
        const auto lit = accent == 2 ? Palette::amber : accent == 1 ? juce::Colour (0xff42a9ff) : Palette::text;

        if (i == current)
        {
            g.setGradientFill (juce::ColourGradient (lit.withAlpha (0.5f), dot.getCentreX(), dot.getCentreY(),
                                                     lit.withAlpha (0.0f), dot.getCentreX() + d, dot.getCentreY(), true));
            g.fillEllipse (dot.expanded (d * 0.8f));
        }
        g.setColour (juce::Colour (0xff1c1d20));
        g.fillEllipse (dot.expanded (2.0f));
        g.setColour (i == current ? lit : lit.withAlpha (0.18f));
        g.fillEllipse (dot);
        g.setColour (accent == 2 ? Palette::gold : Palette::dim.withAlpha (0.6f));
        g.drawEllipse (dot, accent == 2 ? 2.0f : 1.0f);
    }
}

// ---------------------------------------------------------------- editor

GuitarDriveEditor::GuitarDriveEditor (GuitarDriveProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      ampView (p),
      preBoard (p.apvts, Params::prePedals(), { { 0, 1 }, { 2, 3 } }),
      postBoard (p.apvts, Params::postPedals(), { { 0, 1, 2 }, { 3, 4 } }),
      eqView (p.apvts),
      metroView (p),
      tunerView (p),
      stereoAttach (*p.apvts.getParameter (GuitarDriveProcessor::stereoId), [this] (float v) { setStereo (v > 0.5f); }),
      rightInAttach (*p.apvts.getParameter (GuitarDriveProcessor::rightSameId),
                     [this] (float v) { rightUsesIn1 = v > 0.5f; rightInButton.setToggleState (rightUsesIn1, juce::dontSendNotification); })
{
    setLookAndFeel (&look);
    setResizable (true, false);
    addAndMakeVisible (root);
    root.painter = [this] (juce::Graphics& g) { paintRoot (g); };

    static const char* const tabNames[5] = { "PRE FX", "AMP", "EQ", "POST FX", "METRO" };
    for (size_t i = 0; i < tabs.size(); ++i)
    {
        auto& t = tabs[i];
        t.setButtonText (tabNames[i]);
        t.setClickingTogglesState (true);
        t.setRadioGroupId (101);
        t.onClick = [this, i] { setView ((View) i); };
        root.addAndMakeVisible (t);
    }
    tabs[(size_t) View::amp].setToggleState (true, juce::dontSendNotification);

    const auto theme = Themes::neutral();
    inputKnob.init (root, p.apvts, Params::inputKnob().id, Params::inputKnob().label, theme);
    outputKnob.init (root, p.apvts, Params::outputKnob().id, Params::outputKnob().label, theme);

    tunerButton.setClickingTogglesState (true);
    tunerButton.onClick = [this] { setTunerOpen (tunerButton.getToggleState()); };
    root.addAndMakeVisible (tunerButton);

    stereoButton.onClick = [this] { stereoAttach.setValueAsCompleteGesture (stereoMode ? 0.0f : 1.0f); };
    root.addAndMakeVisible (stereoButton);

    // On: the right strip hears input 1 (one guitar through two chains). Off: it hears input 2.
    rightInButton.onClick = [this] { rightInAttach.setValueAsCompleteGesture (rightUsesIn1 ? 0.0f : 1.0f); };
    root.addAndMakeVisible (rightInButton);

    root.addAndMakeVisible (ampView);
    root.addAndMakeVisible (preBoard);
    root.addAndMakeVisible (postBoard);
    root.addAndMakeVisible (eqView);
    root.addAndMakeVisible (metroView);
    root.addAndMakeVisible (tunerView);

    rightInAttach.sendInitialUpdate();
    stereoAttach.sendInitialUpdate();   // also sizes the window
}

GuitarDriveEditor::~GuitarDriveEditor()
{
    proc.setTunerActive (false);   // never leave the output muted behind a closed window
    setLookAndFeel (nullptr);
}

void GuitarDriveEditor::recomputeDesign()
{
    bodyH = stereoMode ? 470 : 340;
    designH = headerH + gap + bodyH + hintH;
}

float GuitarDriveEditor::fitScale() const
{
    // Start at a size that fits the screen; the window can then be resized freely.
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        const float sx = (float) (area.getWidth() - 40) / (float) designW;
        const float sy = (float) (area.getHeight() - 70) / (float) designH;
        return juce::jlimit (0.5f, 1.0f, juce::jmin (sx, sy));
    }
    return 0.7f;
}

void GuitarDriveEditor::keepOnScreen()
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<GuitarDriveEditor> (this)]
    {
        if (safe == nullptr)
            return;

        auto* top = safe->getTopLevelComponent();
        if (top == nullptr || top == safe.getComponent())
            return;

        if (const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (top->getBounds()))
        {
            const auto moved = top->getBounds().constrainedWithin (display->userArea);
            if (moved != top->getBounds())
                top->setBounds (moved);
        }
    });
}

void GuitarDriveEditor::setStereo (bool on)
{
    const bool first = ! sizeInitialised;
    const float oldScale = first ? 0.0f : (float) getWidth() / (float) designW;

    stereoMode = on;
    stereoButton.setToggleState (on, juce::dontSendNotification);
    ampView.setStereo (on);
    recomputeDesign();
    updateVisibility();

    // The smallest size is half scale: everything is scaled together, so it always fits.
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setResizeLimits (juce::roundToInt ((float) designW * 0.5f), juce::roundToInt ((float) designH * 0.5f),
                     juce::roundToInt ((float) designW * 1.6f), juce::roundToInt ((float) designH * 1.6f));

    const float scale = first ? fitScale() : oldScale;
    sizeInitialised = true;
    setSize (juce::roundToInt ((float) designW * scale), juce::roundToInt ((float) designH * scale));
    resized();
    repaint();
    keepOnScreen();
}

void GuitarDriveEditor::setTunerOpen (bool open)
{
    tunerOpen = open;
    tunerButton.setToggleState (open, juce::dontSendNotification);
    proc.setTunerActive (open);
    updateVisibility();
    repaint();
}

void GuitarDriveEditor::setView (View v)
{
    view = v;
    if (tunerOpen)
        setTunerOpen (false);
    updateVisibility();
    repaint();
}

void GuitarDriveEditor::updateVisibility()
{
    const bool normal = ! tunerOpen;
    preBoard.setVisible (normal && view == View::pre);
    ampView.setVisible (normal && view == View::amp);
    eqView.setVisible (normal && view == View::eq);
    postBoard.setVisible (normal && view == View::post);
    metroView.setVisible (normal && view == View::metro);
    tunerView.setVisible (tunerOpen);
    rightInButton.setVisible (stereoMode);
}

void GuitarDriveEditor::resized()
{
    const float scale = (float) getWidth() / (float) designW;
    root.setTransform (juce::AffineTransform::scale (scale));
    root.setBounds (0, 0, designW, designH);
    layout();
}

void GuitarDriveEditor::layout()
{
    const int w = designW;
    titleRect = { 20, 8, 180, 68 };
    headerRect = { 214, 8, 586, 68 };
    bodyRect = { 20, headerH + gap, w - 40, bodyH };

    for (size_t i = 0; i < tabs.size(); ++i)
        tabs[i].setBounds (224 + (int) i * 82, 24, 78, 36);
    inputKnob.place ({ 640, 8, 72, 68 }, 14);
    outputKnob.place ({ 716, 8, 72, 68 }, 14);

    tunerButton.setBounds (w - 20 - 90, 26, 90, 34);
    stereoButton.setBounds (w - 20 - 90 - 8 - 100, 26, 100, 34);
    rightInButton.setBounds (w - 20 - 90 - 8 - 100 - 8 - 130, 26, 130, 34);

    ampView.setBounds (bodyRect);
    preBoard.setBounds (bodyRect);
    eqView.setBounds (bodyRect);
    postBoard.setBounds (bodyRect);
    metroView.setBounds (bodyRect);
    tunerView.setBounds (bodyRect);
    repaint();
}

void GuitarDriveEditor::paintRoot (juce::Graphics& g)
{
    if (frameImage.isNull() || frameImage.getWidth() != designW || frameImage.getHeight() != designH)
        frameImage = Textures::graphite (designW, designH);
    g.drawImageAt (frameImage, 0, 0);

    panelPlate (g, titleRect, 10.0f);
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("GuitarDrive", titleRect.withTrimmedBottom (14), juce::Justification::centred);
    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("AMP  -  PEDALS  -  FX", titleRect.withTrimmedTop (44), juce::Justification::centred);

    panelPlate (g, headerRect, 10.0f);

    const bool pedalView = ! tunerOpen && (view == View::pre || view == View::post || view == View::eq);
    if (pedalView)
    {
        if (carpetImage.isNull() || carpetImage.getWidth() != bodyRect.getWidth() || carpetImage.getHeight() != bodyRect.getHeight())
            carpetImage = Textures::carpet (bodyRect.getWidth(), bodyRect.getHeight());

        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (bodyRect.toFloat(), 12.0f);
            g.reduceClipRegion (clip);
            g.drawImageAt (carpetImage, bodyRect.getX(), bodyRect.getY());
        }
        g.setColour (Palette::silver.withAlpha (0.4f));
        g.drawRoundedRectangle (bodyRect.toFloat().reduced (1.0f), 12.0f, 1.5f);
    }
    else if (tunerOpen || view == View::metro)
    {
        panelPlate (g, bodyRect, 12.0f);
    }

    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (12.0f));
    juce::String hint = "Scroll over a knob to turn it  |  Shift = fine  |  Double-click = reset";
    if (stereoMode && view == View::amp && ! tunerOpen)
        hint += "  |  R USES IN 1: on = right amp hears input 1, off = input 2";
    if (view == View::metro && ! tunerOpen)
        hint = "Scroll over the BPM knob, or tap the TAP button in time";
    if (tunerOpen)
        hint = "Tuner open: output muted. Click TUNER again, or any tab, to go back";
    g.drawText (hint, 24, designH - hintH, designW - 48, hintH - 2, juce::Justification::centredLeft);
}
