#include "PluginEditor.h"

#include <BinaryData.h>
#include <cstring>
#include <functional>
#include <string>

namespace
{
    juce::Image loadEmbeddedLogo()
    {
        return juce::ImageFileFormat::loadFrom (BinaryData::joegarcialogo_png, (size_t) BinaryData::joegarcialogo_pngSize);
    }

    void drawScrew (juce::Graphics& g, juce::Point<float> c, float r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f1f3), c.x - r * 0.4f, c.y - r * 0.5f,
                                                 juce::Colour (0xff6b6f76), c.x + r * 0.6f, c.y + r * 0.7f, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xff2c2e33));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.8f);
        g.drawLine (c.x - r * 0.65f, c.y + r * 0.3f, c.x + r * 0.65f, c.y - r * 0.3f, 1.4f);
    }

    // Brushed-aluminum stompbox body with a soft shadow, bevel and four screws.
    void drawEnclosure (juce::Graphics& g, juce::Rectangle<float> b, const juce::Image& brushed, float screwRadius = 3.6f)
    {
        constexpr float corner = 12.0f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (b.translated (0.0f, 4.0f), corner);
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (b, corner);
            g.reduceClipRegion (clip);
            g.drawImageAt (brushed, 0, 0);
        }
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawRoundedRectangle (b.reduced (1.5f), corner - 1.0f, 1.2f);
        g.setColour (juce::Colour (0xff383b40));
        g.drawRoundedRectangle (b, corner, 1.6f);

        drawScrew (g, { b.getX() + 11.0f, b.getY() + 11.0f }, screwRadius);
        drawScrew (g, { b.getRight() - 11.0f, b.getY() + 11.0f }, screwRadius);
        drawScrew (g, { b.getX() + 11.0f, b.getBottom() - 11.0f }, screwRadius);
        drawScrew (g, { b.getRight() - 11.0f, b.getBottom() - 11.0f }, screwRadius);
    }

    void styleAmpButton (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, Palette::plateLight);
        b.setColour (juce::TextButton::buttonOnColourId, Palette::amber);
        b.setColour (juce::TextButton::textColourOffId, Palette::cream);
        b.setColour (juce::TextButton::textColourOnId, Palette::inkDark);
        b.setColour (juce::ComboBox::outlineColourId, Palette::silver);
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

AmpView::AmpView (GuitarDriveProcessor& p, const juce::Image& logoImage)
    : apvts (p.apvts), logo (logoImage)
{
    const auto& defs = Params::stripKnobs();
    const auto theme = Themes::amp();

    for (int s = 0; s < 2; ++s)
    {
        auto& strip = strips[(size_t) s];

        for (size_t k = 0; k < defs.size(); ++k)
            strip.knobs[k].init (*this, apvts, GuitarDriveProcessor::paramId (s, defs[k].id), defs[k].label, theme);

        // transpose moves in whole semitones: one wheel notch is one semitone
        strip.knobs[1].slider.setWheelFractions (1.0 / 24.0, 1.0 / 24.0);

        strip.name.setText (s == 0 ? "LEFT" : "RIGHT", juce::dontSendNotification);
        strip.name.setJustificationType (juce::Justification::centred);
        strip.name.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        strip.name.setColour (juce::Label::textColourId, Palette::cream);
        addAndMakeVisible (strip.name);

        styleAmpButton (strip.revType);
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

void AmpView::updateVisibility()
{
    for (int s = 0; s < 2; ++s)
    {
        auto& strip = strips[(size_t) s];
        const bool shown = s == 0 || stereo;

        for (auto& k : strip.knobs)
            k.show (shown);

        strip.name.setVisible (shown && stereo);
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

    auto right = content.removeFromRight (108).reduced (6, 0);
    strip.revType.setBounds (right.withSizeKeepingCentre (right.getWidth(), 32));

    const int colWidth = content.getWidth() / numKnobs;
    for (auto& k : strip.knobs)
        k.place (content.removeFromLeft (colWidth));
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

    // black control plate with a chrome edge
    const auto pl = plate[i].toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (pl.translated (0.0f, 3.0f), 8.0f);
    g.setGradientFill (juce::ColourGradient (Palette::plateLight, pl.getX(), pl.getY(), Palette::plate, pl.getX(), pl.getBottom(), false));
    g.fillRoundedRectangle (pl, 8.0f);
    g.setColour (juce::Colour (0xff9a9ca1));
    g.drawRoundedRectangle (pl.reduced (1.0f), 8.0f, 1.8f);

    drawScrew (g, { pl.getX() + 14.0f, pl.getY() + 14.0f }, 4.5f);
    drawScrew (g, { pl.getRight() - 14.0f, pl.getY() + 14.0f }, 4.5f);
    drawScrew (g, { pl.getX() + 14.0f, pl.getBottom() - 14.0f }, 4.5f);
    drawScrew (g, { pl.getRight() - 14.0f, pl.getBottom() - 14.0f }, 4.5f);

    // input jack
    const juce::Point<float> jack (pl.getX() + 49.0f, pl.getY() + 54.0f);
    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("INPUT", juce::Rectangle<float> (jack.x - 30.0f, jack.y - 34.0f, 60.0f, 14.0f).toNearestInt(), juce::Justification::centred);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (jack.x - 15.0f, jack.y - 13.0f, 30.0f, 30.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f3f5), jack.x - 6.0f, jack.y - 8.0f,
                                             juce::Colour (0xff6d7077), jack.x + 8.0f, jack.y + 10.0f, true));
    g.fillEllipse (jack.x - 15.0f, jack.y - 15.0f, 30.0f, 30.0f);
    g.setColour (juce::Colour (0xff0a0a0b));
    g.fillEllipse (jack.x - 8.0f, jack.y - 8.0f, 16.0f, 16.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (jack.x - 8.0f, jack.y - 8.0f, 16.0f, 16.0f, 1.0f);

    // red pilot lamp
    const juce::Point<float> lamp (pl.getX() + 49.0f, pl.getY() + 120.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff3b2a).withAlpha (0.5f), lamp.x, lamp.y,
                                             juce::Colours::transparentBlack, lamp.x + 30.0f, lamp.y, true));
    g.fillEllipse (lamp.x - 30.0f, lamp.y - 30.0f, 60.0f, 60.0f);
    g.setColour (juce::Colour (0xff9a9ca1));
    g.fillEllipse (lamp.x - 15.0f, lamp.y - 15.0f, 30.0f, 30.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff8a70), lamp.x - 4.0f, lamp.y - 5.0f,
                                             juce::Colour (0xffb01408), lamp.x + 6.0f, lamp.y + 7.0f, true));
    g.fillEllipse (lamp.x - 11.0f, lamp.y - 11.0f, 22.0f, 22.0f);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillEllipse (lamp.x - 6.0f, lamp.y - 7.0f, 7.0f, 4.5f);
    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("PILOT", juce::Rectangle<float> (lamp.x - 30.0f, lamp.y + 18.0f, 60.0f, 14.0f).toNearestInt(), juce::Justification::centred);

    // speaker grille cloth with the logo (mono layout only)
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
        g.setColour (juce::Colour (0xff9a9ca1).withAlpha (0.7f));
        g.drawRoundedRectangle (gr.reduced (4.0f), 4.0f, 1.0f);

        const auto badge = juce::Rectangle<float> (250.0f, juce::jmin (86.0f, gr.getHeight() - 12.0f)).withCentre (gr.getCentre());
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (badge.translated (0.0f, 2.0f), 8.0f);
        g.setColour (Palette::plate);
        g.fillRoundedRectangle (badge, 8.0f);
        g.setColour (juce::Colour (0xff9a9ca1));
        g.drawRoundedRectangle (badge.reduced (1.0f), 8.0f, 1.5f);
        if (logo.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (logo, badge.reduced (10.0f, 4.0f), juce::RectanglePlacement::centred);
        }
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

    if (onId == "wah_on" || onId == "wah_auto") return purple;
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

PedalPanel::PedalPanel (juce::AudioProcessorValueTreeState& apvts, const Params::PedalDef& d, int strip)
    : def (d), colours (coloursFor (d.onId)), footSwitch (colours.led)
{
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, GuitarDriveProcessor::paramId (strip, def.onId), footSwitch);
    addAndMakeVisible (footSwitch);

    const auto theme = Themes::pedal (colours.knob);
    for (const auto& k : def.knobs)
    {
        knobs.push_back (std::make_unique<KnobUI>());
        knobs.back()->init (*this, apvts, GuitarDriveProcessor::paramId (strip, k.id), k.label, theme);
    }
}

void PedalPanel::paint (juce::Graphics& g)
{
    if (brushed.isNull() || brushed.getWidth() != getWidth() || brushed.getHeight() != getHeight())
        brushed = Textures::brushedAluminum (getWidth(), getHeight(), (int) (std::hash<std::string> {} (def.title) & 0xffff));

    drawEnclosure (g, getLocalBounds().toFloat().reduced (4.0f), brushed);

    // silkscreened name with a stripe in the pedal's color
    g.setColour (Palette::inkDark);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (def.title, titleArea, juce::Justification::centred);
    g.setColour (colours.led);
    g.fillRoundedRectangle ((float) titleArea.getCentreX() - 23.0f, (float) titleArea.getBottom() - 1.0f, 46.0f, 4.0f, 2.0f);
}

void PedalPanel::resized()
{
    auto area = getLocalBounds().reduced (16, 14);
    titleArea = area.removeFromTop (28);

    const int footSwitchHeight = juce::jlimit (96, 140, area.getHeight() / 3);
    footSwitch.setBounds (area.removeFromBottom (footSwitchHeight).withSizeKeepingCentre (juce::jmin (140, area.getWidth()), footSwitchHeight));
    area.removeFromTop (4);

    const int n = (int) knobs.size();
    const int cols = n <= 2 ? 1 : 2;
    const int rows = (n + cols - 1) / cols;
    const int cellW = area.getWidth() / cols, cellH = area.getHeight() / rows;

    for (int k = 0; k < n; ++k)
    {
        const int row = k / cols, col = k % cols;
        const int inRow = juce::jmin (cols, n - row * cols);   // a short last row is centred
        const int x0 = area.getX() + (cols - inRow) * cellW / 2 + col * cellW;
        knobs[(size_t) k]->place ({ x0, area.getY() + row * cellH, cellW, cellH }, 16);
    }
}

PedalBoard::PedalBoard (juce::AudioProcessorValueTreeState& apvts, const std::vector<Params::PedalDef>& defs, int strip)
{
    for (const auto& d : defs)
    {
        panels.push_back (std::make_unique<PedalPanel> (apvts, d, strip));
        addAndMakeVisible (*panels.back());
    }
}

void PedalBoard::resized()
{
    constexpr int space = 14;
    int total = space * ((int) panels.size() - 1);
    for (auto& p : panels)
        total += p->preferredWidth();

    auto row = getLocalBounds().reduced (0, 8).withSizeKeepingCentre (total, getHeight() - 16);
    for (auto& p : panels)
    {
        p->setBounds (row.removeFromLeft (p->preferredWidth()));
        row.removeFromLeft (space);
    }
}

// ---------------------------------------------------------------- wah

WahTreadle::WahTreadle (juce::AudioProcessorValueTreeState& apvts, int strip)
    : attachment (*apvts.getParameter (GuitarDriveProcessor::paramId (strip, Params::wahPosKnob().id)),
                  [this] (float v) { value = v; repaint(); })
{
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> WahTreadle::padArea() const
{
    return getLocalBounds().toFloat().reduced (14.0f, 12.0f);
}

void WahTreadle::setFromMouse (const juce::MouseEvent& e)
{
    const auto pad = padArea();
    const float t = 1.0f - juce::jlimit (0.0f, 1.0f, (e.position.y - pad.getY()) / pad.getHeight());
    attachment.setValueAsPartOfGesture (t * 10.0f);
}

void WahTreadle::mouseDown (const juce::MouseEvent& e)
{
    attachment.beginGesture();
    setFromMouse (e);
}

void WahTreadle::mouseDrag (const juce::MouseEvent& e) { setFromMouse (e); }
void WahTreadle::mouseUp (const juce::MouseEvent&) { attachment.endGesture(); }
void WahTreadle::mouseDoubleClick (const juce::MouseEvent&) { attachment.setValueAsCompleteGesture (5.0f); }

void WahTreadle::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const float delta = (wheel.isReversed ? -1.0f : 1.0f) * wheel.deltaY;
    if (delta == 0.0f)
        return;

    const float step = e.mods.isShiftDown() ? 0.05f : 0.3f;   // out of 10
    attachment.setValueAsCompleteGesture (juce::jlimit (0.0f, 10.0f, value + (delta > 0 ? step : -step)));
}

void WahTreadle::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);

    // chrome housing
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8eaee), r.getX(), 0.0f, juce::Colour (0xff7c8088), r.getRight(), 0.0f, false));
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (juce::Colour (0xff2a2c31));
    g.drawRoundedRectangle (r, 14.0f, 2.0f);

    // rubber pad with ribs
    const auto pad = padArea();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e3034), pad.getX(), pad.getY(), juce::Colour (0xff101113), pad.getX(), pad.getBottom(), false));
    g.fillRoundedRectangle (pad, 10.0f);
    for (float y = pad.getY() + 8.0f; y < pad.getBottom() - 8.0f; y += 11.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (pad.getX() + 8.0f, y, pad.getWidth() - 16.0f, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRect (pad.getX() + 8.0f, y + 3.0f, pad.getWidth() - 16.0f, 2.0f);
    }

    // a soft highlight rides up and down with the pedal, as if it were rocking
    const float t = value / 10.0f;
    const float hy = pad.getBottom() - t * pad.getHeight();
    juce::ColourGradient band (juce::Colours::white.withAlpha (0.0f), 0.0f, hy - 60.0f, juce::Colours::white.withAlpha (0.0f), 0.0f, hy + 60.0f, false);
    band.addColour (0.5, juce::Colours::white.withAlpha (0.16f));
    g.setGradientFill (band);
    g.fillRoundedRectangle (pad, 10.0f);

    // position LEDs down the right edge
    for (int i = 0; i < 10; ++i)
    {
        const float y = pad.getBottom() - 16.0f - (float) i * ((pad.getHeight() - 32.0f) / 9.0f);
        const bool lit = (float) i + 0.5f <= value;
        g.setColour (lit ? juce::Colour (0xffb56bff) : juce::Colour (0xff3a3c41));
        g.fillEllipse (pad.getRight() - 20.0f, y - 4.0f, 8.0f, 8.0f);
    }

    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("TOE", pad.withHeight (22.0f).toNearestInt(), juce::Justification::centred);
    g.drawText ("HEEL", pad.withTop (pad.getBottom() - 22.0f).toNearestInt(), juce::Justification::centred);
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText (juce::String (value, 1), pad.withSizeKeepingCentre (pad.getWidth(), 30.0f).toNearestInt(), juce::Justification::centred);
}

WahPedal::WahPedal (juce::AudioProcessorValueTreeState& apvts, int strip)
    : treadle (apvts, strip)
{
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, GuitarDriveProcessor::paramId (strip, Params::wahOn), footSwitch);
    addAndMakeVisible (treadle);
    addAndMakeVisible (footSwitch);
}

void WahPedal::resized()
{
    auto area = getLocalBounds().reduced (18, 14);
    titleArea = area.removeFromTop (28);
    const int footSwitchHeight = juce::jlimit (96, 140, area.getHeight() / 3);
    footSwitch.setBounds (area.removeFromBottom (footSwitchHeight).withSizeKeepingCentre (juce::jmin (140, area.getWidth()), footSwitchHeight));
    treadle.setBounds (area.reduced (6, 4));
}

void WahPedal::paint (juce::Graphics& g)
{
    if (brushed.isNull() || brushed.getWidth() != getWidth() || brushed.getHeight() != getHeight())
        brushed = Textures::brushedAluminum (getWidth(), getHeight(), 4242);

    drawEnclosure (g, getLocalBounds().toFloat().reduced (4.0f), brushed);
    g.setColour (Palette::inkDark);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("WAH", titleArea, juce::Justification::centred);
    g.setColour (juce::Colour (0xffb56bff));
    g.fillRoundedRectangle ((float) titleArea.getCentreX() - 23.0f, (float) titleArea.getBottom() - 1.0f, 46.0f, 4.0f, 2.0f);
}

WahView::WahView (juce::AudioProcessorValueTreeState& apvts, int strip)
    : wah (apvts, strip), autoWah (apvts, Params::wahAutoPedal(), strip)
{
    addAndMakeVisible (wah);
    addAndMakeVisible (autoWah);
}

void WahView::resized()
{
    constexpr int wahWidth = 300, space = 24;
    const int total = wahWidth + space + autoWah.preferredWidth();
    auto row = getLocalBounds().reduced (0, 8).withSizeKeepingCentre (total, getHeight() - 16);
    wah.setBounds (row.removeFromLeft (wahWidth));
    row.removeFromLeft (space);
    autoWah.setBounds (row);
}

// ---------------------------------------------------------------- equalizer

EqView::EqView (juce::AudioProcessorValueTreeState& apvts, int strip)
{
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, GuitarDriveProcessor::paramId (strip, Params::eqOn), footSwitch);
    addAndMakeVisible (footSwitch);

    const juce::Colour cap (0xffbcd8ec);   // creamy blue fader caps
    for (int b = 0; b < Params::numEqBands; ++b)
    {
        auto& band = bands[(size_t) b];
        band.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GuitarDriveProcessor::paramId (strip, Params::eqId (b)), band.fader);
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
    auto area = getLocalBounds().reduced (22, 16);
    footSwitch.setBounds (area.removeFromRight (88).removeFromTop (118));
    area.removeFromRight (10);

    titleArea = area.removeFromTop (24);
    area.removeFromTop (4);
    curveArea = area.removeFromTop (juce::jmin (110, area.getHeight() / 3));
    area.removeFromTop (6);

    auto faders = getLocalBounds().reduced (22, 16).withTop (area.getY());
    const int colWidth = faders.getWidth() / Params::numEqBands;
    for (auto& band : bands)
    {
        auto col = faders.removeFromLeft (colWidth);
        band.label.setBounds (col.removeFromTop (20));
        band.fader.setBounds (col.reduced (6, 0));
    }
    updateCurve();
}

void EqView::paint (juce::Graphics& g)
{
    if (brushed.isNull() || brushed.getWidth() != getWidth() || brushed.getHeight() != getHeight())
        brushed = Textures::brushedAluminum (getWidth(), getHeight(), 99);

    drawEnclosure (g, getLocalBounds().toFloat().reduced (6.0f), brushed, 3.8f);

    g.setColour (Palette::inkDark);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
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

// ---------------------------------------------------------------- footer: tuner button and metronome

FooterBar::FooterBar (GuitarDriveProcessor& p)
    : proc (p),
      bpmAttach (*p.apvts.getParameter (Params::metroBpm), [this] (float v) { bpmBox.setNumber ((int) std::lround (v)); }),
      beatsAttach (*p.apvts.getParameter (Params::metroBeats),
                   [this] (float v) { beatsBox.setNumber ((int) std::lround (v)); repaint (dotsArea); }),
      noteAttach (*p.apvts.getParameter (Params::metroNote),
                  [this] (float v) { noteBox.setNumber (Metronome::noteValue ((int) std::lround (v))); repaint (dotsArea); })
{
    tunerButton.setClickingTogglesState (true);
    addAndMakeVisible (tunerButton);

    playAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Params::metroOn, playButton);
    addAndMakeVisible (playButton);

    const auto caption = [this] (juce::Label& l, const juce::String& text, juce::Justification j = juce::Justification::centred)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (j);
        l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, Palette::dim);
        addAndMakeVisible (l);
    };
    caption (tempoLabel, "TEMPO (BPM)");
    caption (sigLabel, "TIME SIGNATURE");
    caption (slashLabel, "/");
    slashLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    slashLabel.setColour (juce::Label::textColourId, Palette::text);

    bpmBox.onUserValue = [this] (int v) { bpmAttach.setValueAsCompleteGesture ((float) v); };
    beatsBox.onUserValue = [this] (int v) { beatsAttach.setValueAsCompleteGesture ((float) v); };
    noteBox.onUserValue = [this] (int v)
    {
        const int index = Metronome::noteIndexFor (v);   // only 2, 4, 8 and 16 are note values
        noteAttach.setValueAsCompleteGesture ((float) index);
        noteBox.showNumber (Metronome::noteValue (index));
    };
    addAndMakeVisible (bpmBox);
    addAndMakeVisible (beatsBox);
    addAndMakeVisible (noteBox);

    tapButton.onClick = [this] { tap(); };
    addAndMakeVisible (tapButton);

    vol.init (*this, p.apvts, Params::metroVol, "CLICK", Themes::neutral());

    bpmAttach.sendInitialUpdate();
    beatsAttach.sendInitialUpdate();
    noteAttach.sendInitialUpdate();
    startTimerHz (25);
}

void FooterBar::tap()
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
        const int bpm = (int) std::lround (juce::jlimit (30.0, 260.0, 60000.0 / averageMs));
        bpmAttach.setValueAsCompleteGesture ((float) bpm);
        bpmBox.showNumber (bpm);
    }
}

void FooterBar::resized()
{
    auto area = getLocalBounds().reduced (18, 10);

    tunerButton.setBounds (area.removeFromLeft (96).withSizeKeepingCentre (96, 40));
    area.removeFromLeft (26);
    playButton.setBounds (area.removeFromLeft (56).withSizeKeepingCentre (56, 56));
    area.removeFromLeft (22);

    auto tempo = area.removeFromLeft (110);
    tempoLabel.setBounds (tempo.removeFromTop (16));
    bpmBox.setBounds (tempo.withSizeKeepingCentre (96, 38));
    area.removeFromLeft (8);
    tapButton.setBounds (area.removeFromLeft (72).withSizeKeepingCentre (72, 38).translated (0, 8));
    area.removeFromLeft (30);

    auto sig = area.removeFromLeft (150);
    sigLabel.setBounds (sig.removeFromTop (16));
    auto row = sig.withSizeKeepingCentre (136, 38);
    beatsBox.setBounds (row.removeFromLeft (56));
    slashLabel.setBounds (row.removeFromLeft (24));
    noteBox.setBounds (row.removeFromLeft (56));
    area.removeFromLeft (28);

    vol.place (area.removeFromRight (92), 16);
    area.removeFromRight (16);
    dotsArea = area.withSizeKeepingCentre (area.getWidth(), 24);
}

void FooterBar::paint (juce::Graphics& g)
{
    panelPlate (g, getLocalBounds(), 12.0f);

    // one light per pulse of the bar; the strong pulse is amber, the medium ones blue
    const int beats = Metronome::clampBeats ((int) std::lround (proc.apvts.getRawParameterValue (Params::metroBeats)->load()));
    const int note = Metronome::noteValue ((int) std::lround (proc.apvts.getRawParameterValue (Params::metroNote)->load()));
    const int current = proc.getMetronome().getPulse();
    constexpr float d = 16.0f, space = 6.0f;
    const float y = (float) dotsArea.getCentreY();

    for (int i = 0; i < beats; ++i)
    {
        const auto dot = juce::Rectangle<float> (d, d).withCentre ({ (float) dotsArea.getX() + d * 0.5f + (float) i * (d + space), y });
        const int accent = Metronome::accentFor (beats, note, i);
        const auto lit = accent == 2 ? Palette::amber : accent == 1 ? juce::Colour (0xff42a9ff) : Palette::text;

        if (i == current)
        {
            g.setGradientFill (juce::ColourGradient (lit.withAlpha (0.5f), dot.getCentreX(), dot.getCentreY(),
                                                     lit.withAlpha (0.0f), dot.getCentreX() + d, dot.getCentreY(), true));
            g.fillEllipse (dot.expanded (d * 0.7f));
        }
        g.setColour (juce::Colour (0xff1c1d20));
        g.fillEllipse (dot.expanded (2.0f));
        g.setColour (i == current ? lit : lit.withAlpha (0.2f));
        g.fillEllipse (dot);
        g.setColour (accent == 2 ? Palette::silver : Palette::dim.withAlpha (0.6f));
        g.drawEllipse (dot, accent == 2 ? 1.8f : 1.0f);
    }
}

// ---------------------------------------------------------------- presets

PresetBar::PresetBar (GuitarDriveProcessor& p)
    : proc (p)
{
    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };
    nameButton.onClick = [this] { showMenu(); };
    saveButton.onClick = [this] { saveCurrent(); };
    newButton.onClick = [this] { saveAsNew(); };
    deleteButton.onClick = [this] { askDelete(); };

    nameButton.setColour (juce::TextButton::buttonColourId, Palette::graphite);
    nameButton.setColour (juce::TextButton::textColourOffId, Palette::amber);

    for (auto* b : { &prevButton, &nextButton, &nameButton, &saveButton, &newButton, &deleteButton })
        addAndMakeVisible (*b);

    refresh();
    startTimerHz (4);
}

void PresetBar::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    prevButton.setBounds (area.removeFromLeft (34));
    area.removeFromLeft (6);
    deleteButton.setBounds (area.removeFromRight (78));
    area.removeFromRight (6);
    newButton.setBounds (area.removeFromRight (56));
    area.removeFromRight (6);
    saveButton.setBounds (area.removeFromRight (70));
    area.removeFromRight (6);
    nextButton.setBounds (area.removeFromRight (34));
    area.removeFromRight (6);
    nameButton.setBounds (area);
}

void PresetBar::paint (juce::Graphics& g)
{
    panelPlate (g, getLocalBounds(), 10.0f);
}

void PresetBar::refresh()
{
    auto& presets = proc.presets;
    const auto name = presets.currentName();
    const bool dirty = presets.isDirty();

    const juce::String text = name.isEmpty() ? juce::String ("No preset selected") : name + (dirty ? " *" : "");
    if (text != shownText)
    {
        shownText = text;
        nameButton.setButtonText (text);
    }
    saveButton.setEnabled (name.isEmpty() || dirty);   // with no preset selected, SAVE creates one
    deleteButton.setEnabled (name.isNotEmpty());
}

void PresetBar::step (int direction)
{
    const auto names = proc.presets.names();
    if (names.isEmpty())
        return;

    const int current = names.indexOf (proc.presets.currentName());
    const int next = current < 0 ? (direction > 0 ? 0 : names.size() - 1) : (current + direction + names.size()) % names.size();
    proc.presets.load (names[next]);
    refresh();
}

void PresetBar::showMenu()
{
    const auto names = proc.presets.names();
    const auto current = proc.presets.currentName();

    juce::PopupMenu menu;
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i], true, names[i] == current);
    if (names.isEmpty())
        menu.addItem (1000, "(no presets yet)", false);
    menu.addSeparator();
    menu.addItem (1001, "Save as new preset...");
    menu.addItem (1002, "Delete current preset...", current.isNotEmpty());
    menu.addItem (1003, "Show presets folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton),
                        [safe = juce::Component::SafePointer<PresetBar> (this), names] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result >= 1 && result <= names.size())
                                safe->proc.presets.load (names[result - 1]);
                            else if (result == 1001)
                                safe->saveAsNew();
                            else if (result == 1002)
                                safe->askDelete();
                            else if (result == 1003)
                                safe->proc.presets.getFolder().revealToUser();
                            safe->refresh();
                        });
}

void PresetBar::saveCurrent()
{
    const auto name = proc.presets.currentName();
    if (name.isEmpty())
        saveAsNew();
    else
        proc.presets.save (name);
    refresh();
}

void PresetBar::saveAsNew()
{
    askName ("New preset", {}, [this] (const juce::String& name) { saveNamed (name); });
}

void PresetBar::saveNamed (const juce::String& name)
{
    const auto clean = PresetManager::sanitise (name);
    if (clean.isEmpty())
        return;

    if (proc.presets.names().contains (clean, true))   // already there: ask before replacing it
    {
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::WarningIcon)
                                          .withTitle ("Replace preset?")
                                          .withMessage ("A preset called \"" + clean + "\" already exists. Replace it with the current settings?")
                                          .withButton ("Replace")
                                          .withButton ("Cancel")
                                          .withAssociatedComponent (this),
                                      [safe = juce::Component::SafePointer<PresetBar> (this), clean] (int result)
                                      {
                                          if (safe != nullptr && result == 1)
                                          {
                                              safe->proc.presets.save (clean);
                                              safe->refresh();
                                          }
                                      });
        return;
    }

    proc.presets.save (clean);
    refresh();
}

void PresetBar::askDelete()
{
    const auto name = proc.presets.currentName();
    if (name.isEmpty())
        return;

    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("Delete preset")
                                      .withMessage ("Delete the preset \"" + name + "\"? This cannot be undone.")
                                      .withButton ("Delete")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safe = juce::Component::SafePointer<PresetBar> (this), name] (int result)
                                  {
                                      if (safe != nullptr && result == 1)
                                      {
                                          safe->proc.presets.remove (name);
                                          safe->refresh();
                                      }
                                  });
}

void PresetBar::askName (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> onName)
{
    auto* window = new juce::AlertWindow (title, "Type a name for the preset.", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", initial);
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->enterModalState (true,
                             juce::ModalCallbackFunction::create ([window, onName] (int result)
                             {
                                 if (result == 1)
                                     onName (window->getTextEditorContents ("name").trim());
                             }),
                             true);

    // ready to type as soon as the dialog opens
    if (auto* editor = window->getTextEditor ("name"))
    {
        editor->grabKeyboardFocus();
        editor->selectAll();
    }
}

// ---------------------------------------------------------------- editor

GuitarDriveEditor::GuitarDriveEditor (GuitarDriveProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      logoImage (loadEmbeddedLogo()),
      ampView (p, logoImage),
      footer (p),
      presetBar (p),
      tunerView (p),
      stereoAttach (*p.apvts.getParameter (GuitarDriveProcessor::stereoId), [this] (float v) { setStereo (v > 0.5f); }),
      rightInAttach (*p.apvts.getParameter (GuitarDriveProcessor::rightSameId),
                     [this] (float v) { rightUsesIn1 = v > 0.5f; rightInButton.setToggleState (rightUsesIn1, juce::dontSendNotification); }),
      soloAttach (*p.apvts.getParameter (GuitarDriveProcessor::soloId),
                  [this] (float v) { soloState = (int) std::lround (v); updateSoloButtons(); })
{
    setLookAndFeel (&look);
    setResizable (true, false);
    addAndMakeVisible (root);
    root.painter = [this] (juce::Graphics& g) { paintRoot (g); };

    static const char* const tabNames[5] = { "WAH", "PRE FX", "AMP", "EQ", "POST FX" };
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

    // pan, solo and mute for both channels sit in the header next to the mono / stereo switch
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& c = channels[(size_t) ch];
        c.pan.init (root, p.apvts, GuitarDriveProcessor::paramId (ch, GuitarDriveProcessor::panId), "PAN", theme);

        c.name.setText (ch == 0 ? "LEFT" : "RIGHT", juce::dontSendNotification);
        c.name.setJustificationType (juce::Justification::centred);
        c.name.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        c.name.setColour (juce::Label::textColourId, Palette::text);
        root.addAndMakeVisible (c.name);

        c.mute.setColour (juce::TextButton::buttonOnColourId, Palette::red);
        c.mute.setColour (juce::TextButton::textColourOnId, Palette::text);
        root.addAndMakeVisible (c.solo);
        root.addAndMakeVisible (c.mute);
    }
    // Solo and mute are linked across the channels: soloing one side is the same as muting the other.
    channels[0].solo.onClick = [this] { setSolo (soloState == 1 ? 0 : 1); };
    channels[1].mute.onClick = [this] { setSolo (soloState == 1 ? 0 : 1); };
    channels[1].solo.onClick = [this] { setSolo (soloState == 2 ? 0 : 2); };
    channels[0].mute.onClick = [this] { setSolo (soloState == 2 ? 0 : 2); };

    stereoButton.onClick = [this] { stereoAttach.setValueAsCompleteGesture (stereoMode ? 0.0f : 1.0f); };
    root.addAndMakeVisible (stereoButton);

    // On: the right channel hears input 1 (one guitar through two chains). Off: it hears input 2.
    rightInButton.onClick = [this] { rightInAttach.setValueAsCompleteGesture (rightUsesIn1 ? 0.0f : 1.0f); };
    root.addAndMakeVisible (rightInButton);

    root.addAndMakeVisible (ampView);
    for (int ch = 0; ch < 2; ++ch)
    {
        wahViews[(size_t) ch] = std::make_unique<WahView> (p.apvts, ch);
        preBoards[(size_t) ch] = std::make_unique<PedalBoard> (p.apvts, Params::prePedals(), ch);
        eqViews[(size_t) ch] = std::make_unique<EqView> (p.apvts, ch);
        postBoards[(size_t) ch] = std::make_unique<PedalBoard> (p.apvts, Params::postPedals(), ch);
        root.addAndMakeVisible (*wahViews[(size_t) ch]);
        root.addAndMakeVisible (*preBoards[(size_t) ch]);
        root.addAndMakeVisible (*eqViews[(size_t) ch]);
        root.addAndMakeVisible (*postBoards[(size_t) ch]);
    }
    root.addAndMakeVisible (footer);
    root.addAndMakeVisible (presetBar);
    root.addAndMakeVisible (tunerView);

    footer.tunerButton.onClick = [this] { setTunerOpen (footer.tunerButton.getToggleState()); };

    rightInAttach.sendInitialUpdate();
    soloAttach.sendInitialUpdate();
    stereoAttach.sendInitialUpdate();   // also sizes the window
}

GuitarDriveEditor::~GuitarDriveEditor()
{
    proc.setTunerActive (false);   // never leave the output muted behind a closed window
    setLookAndFeel (nullptr);
}

void GuitarDriveEditor::recomputeDesign()
{
    bodyH = stereoMode ? 2 * stereoHalfH + gap : monoBodyH;
    designH = presetBarH + gap + headerH + gap + bodyH + gap + footerH + hintH;
}

float GuitarDriveEditor::fitScale() const
{
    // Start at a size that fits the screen; the window can then be resized freely.
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userArea;
        const float sx = (float) (area.getWidth() - 40) / (float) designW;
        const float sy = (float) (area.getHeight() - 70) / (float) designH;
        return juce::jlimit (0.4f, 1.0f, juce::jmin (sx, sy));
    }
    return 0.6f;
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

    // The smallest size is 40% scale: everything is scaled together, so it always fits.
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setResizeLimits (juce::roundToInt ((float) designW * 0.4f), juce::roundToInt ((float) designH * 0.4f),
                     juce::roundToInt ((float) designW * 1.6f), juce::roundToInt ((float) designH * 1.6f));

    // keep the current size, but never let the taller stereo layout run off the screen
    const float scale = first ? fitScale() : juce::jmin (oldScale, fitScale());
    sizeInitialised = true;
    setSize (juce::roundToInt ((float) designW * scale), juce::roundToInt ((float) designH * scale));
    resized();
    repaint();
    keepOnScreen();
}

void GuitarDriveEditor::setTunerOpen (bool open)
{
    tunerOpen = open;
    footer.tunerButton.setToggleState (open, juce::dontSendNotification);
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

void GuitarDriveEditor::setSolo (int state)
{
    soloAttach.setValueAsCompleteGesture ((float) state);
}

void GuitarDriveEditor::updateSoloButtons()
{
    channels[0].solo.setToggleState (soloState == 1, juce::dontSendNotification);
    channels[1].mute.setToggleState (soloState == 1, juce::dontSendNotification);
    channels[1].solo.setToggleState (soloState == 2, juce::dontSendNotification);
    channels[0].mute.setToggleState (soloState == 2, juce::dontSendNotification);
}

void GuitarDriveEditor::updateVisibility()
{
    const bool normal = ! tunerOpen;
    for (size_t ch = 0; ch < 2; ++ch)
    {
        const bool shown = normal && (ch == 0 || stereoMode);   // in stereo mode every view is split: left above right
        wahViews[ch]->setVisible (shown && view == View::wah);
        preBoards[ch]->setVisible (shown && view == View::pre);
        eqViews[ch]->setVisible (shown && view == View::eq);
        postBoards[ch]->setVisible (shown && view == View::post);

        channels[ch].pan.show (stereoMode);
        channels[ch].name.setVisible (stereoMode);
        channels[ch].solo.setVisible (stereoMode);
        channels[ch].mute.setVisible (stereoMode);
    }
    ampView.setVisible (normal && view == View::amp);
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

juce::Rectangle<int> GuitarDriveEditor::channelRect (int channel) const
{
    if (! stereoMode)
        return bodyRect;

    const int half = (bodyRect.getHeight() - gap) / 2;
    return { bodyRect.getX(), bodyRect.getY() + channel * (half + gap), bodyRect.getWidth(), half };
}

void GuitarDriveEditor::layout()
{
    const int w = designW;
    const int hy = presetBarH + gap;   // top of the header row, under the preset bar
    presetBar.setBounds ((w - 680) / 2, 4, 680, 38);

    titleRect = { 20, hy + 8, 150, 88 };
    headerRect = { 186, hy + 8, 636, 88 };
    channelPlateRect = { 840, hy + 8, 272, 88 };
    bodyRect = { 20, hy + headerH + gap, w - 40, bodyH };
    footerRect = { 20, bodyRect.getBottom() + gap, w - 40, footerH };

    for (size_t i = 0; i < tabs.size(); ++i)
        tabs[i].setBounds (196 + (int) i * 90, hy + 34, 84, 36);
    inputKnob.place ({ 662, hy + 18, 72, 68 }, 14);
    outputKnob.place ({ 738, hy + 18, 72, 68 }, 14);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto& c = channels[(size_t) ch];
        const int gx = 848 + ch * 130;
        c.pan.place ({ gx, hy + 18, 60, 68 }, 14);
        c.name.setBounds (gx + 64, hy + 20, 58, 14);
        c.solo.setBounds (gx + 64, hy + 36, 58, 22);
        c.mute.setBounds (gx + 64, hy + 60, 58, 22);
    }

    stereoButton.setBounds (w - 20 - 100, hy + 35, 100, 34);
    rightInButton.setBounds (w - 20 - 100 - 8 - 110, hy + 35, 110, 34);

    ampView.setBounds (bodyRect);
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto r = channelRect (ch).withTrimmedLeft (stereoMode ? sideTagW : 0);
        wahViews[(size_t) ch]->setBounds (r);
        preBoards[(size_t) ch]->setBounds (r);
        eqViews[(size_t) ch]->setBounds (r);
        postBoards[(size_t) ch]->setBounds (r);
    }
    footer.setBounds (footerRect);
    tunerView.setBounds (bodyRect);
    repaint();
}

void GuitarDriveEditor::paintRoot (juce::Graphics& g)
{
    if (frameImage.isNull() || frameImage.getWidth() != designW || frameImage.getHeight() != designH)
        frameImage = Textures::graphite (designW, designH);
    g.drawImageAt (frameImage, 0, 0);

    // logo plate: just the artist logo
    panelPlate (g, titleRect, 10.0f);
    if (logoImage.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (logoImage, titleRect.reduced (10, 5).toFloat(), juce::RectanglePlacement::centred);
    }

    panelPlate (g, headerRect, 10.0f);
    if (stereoMode)
        panelPlate (g, channelPlateRect, 10.0f);

    const bool pedalView = ! tunerOpen && view != View::amp;
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

        if (stereoMode)   // left channel above, right channel below
        {
            g.setColour (Palette::silver.withAlpha (0.35f));
            g.drawHorizontalLine (channelRect (1).getY() - gap / 2, (float) bodyRect.getX() + 10.0f, (float) bodyRect.getRight() - 10.0f);

            g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            for (int ch = 0; ch < 2; ++ch)
            {
                auto tag = channelRect (ch).withWidth (sideTagW);
                g.setColour (Palette::silver.withAlpha (0.85f));
                g.drawText (ch == 0 ? "LEFT" : "RIGHT", tag, juce::Justification::centred);
            }
        }
    }
    else if (tunerOpen)
    {
        panelPlate (g, bodyRect, 12.0f);
    }

    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (12.0f));
    juce::String hint = "Scroll over a knob to turn it  |  Shift = fine  |  Double-click = reset";
    if (view == View::wah && ! tunerOpen)
        hint = "Drag or scroll over the treadle to sweep the wah  |  AUTO WAH switch follows how hard you pick";
    if (stereoMode && ! tunerOpen)
        hint += "  |  Stereo: left channel on top, right channel below";
    if (tunerOpen)
        hint = "Tuner open: output muted. Click TUNER again, or any tab, to go back";
    g.drawText (hint, 24, designH - hintH, designW - 48, hintH - 2, juce::Justification::centredLeft);
}
