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

    bool fontInstalled (const juce::String& name)
    {
        static const juce::StringArray names = juce::Font::findAllTypefaceNames();
        return names.contains (name);
    }

    // script lettering for the pedal names
    juce::Font scriptFont (float height)
    {
        if (fontInstalled ("Brush Script MT"))
            return juce::Font (juce::FontOptions ("Brush Script MT", height, juce::Font::italic));
        return juce::Font (juce::FontOptions (height, juce::Font::bold | juce::Font::italic));
    }

    juce::String titleCase (const juce::String& text)
    {
        juce::StringArray words;
        words.addTokens (text.toLowerCase(), " ", "");
        for (auto& w : words)
            w = w.containsAnyOf ("0123456789") ? w.toUpperCase()   // model names like TS9 stay in capitals
                                               : w.substring (0, 1).toUpperCase() + w.substring (1);
        return words.joinIntoString (" ");
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

    // A pedal seen slightly from above: the colored enclosure shows as a rim and a deeper edge along the bottom,
    // with a brushed aluminum face on top.
    constexpr float baseDepth = 18.0f;

    juce::Rectangle<float> pedalFace (juce::Rectangle<float> bounds)
    {
        return bounds.reduced (6.0f).withTrimmedBottom (baseDepth);
    }

    void drawEnclosure (juce::Graphics& g, juce::Rectangle<float> b, const juce::Image& brushed, juce::Colour side)
    {
        // soft shadow on the floor, deepest right under the pedal
        for (int i = 6; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.07f));
            g.fillRoundedRectangle (b.translated (0.0f, 4.0f + (float) i).expanded ((float) i * 0.8f - 2.0f, 0.0f), 18.0f + (float) i);
        }

        // die-cast body: the colored walls, lit from above
        juce::Path body;
        body.addRoundedRectangle (b, 18.0f);
        g.setGradientFill (juce::ColourGradient (side.brighter (0.35f), b.getX(), b.getY(), side.darker (0.35f), b.getX(), b.getBottom(), false));
        g.fillPath (body);
        {
            // the front wall, seen from slightly above: darker, with a lit lip where it meets the top
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (body);
            const auto front = b.withTop (b.getBottom() - baseDepth - 4.0f);
            g.setGradientFill (juce::ColourGradient (side.darker (0.25f), front.getX(), front.getY(), side.darker (0.9f), front.getX(), front.getBottom(), false));
            g.fillRect (front);
            g.setColour (side.brighter (0.6f).withAlpha (0.55f));
            g.drawHorizontalLine ((int) front.getY(), front.getX(), front.getRight());
            // side walls catch less light
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.25f), b.getX(), 0.0f, juce::Colours::transparentBlack, b.getX() + 14.0f, 0.0f, false));
            g.fillRect (b.withWidth (14.0f));
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.3f), b.getRight(), 0.0f, juce::Colours::transparentBlack, b.getRight() - 14.0f, 0.0f, false));
            g.fillRect (b.withLeft (b.getRight() - 14.0f));
        }
        g.setColour (juce::Colours::white.withAlpha (0.35f));                      // highlight along the top edge
        g.drawRoundedRectangle (b.reduced (1.5f).withTrimmedBottom (b.getHeight() * 0.5f), 17.0f, 1.2f);
        g.setColour (side.darker (0.85f));
        g.drawRoundedRectangle (b.reduced (0.5f), 18.0f, 1.0f);

        // brushed aluminum top plate, sitting slightly proud of the body
        const auto face = pedalFace (b);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (face.translated (0.0f, 2.5f).expanded (1.0f), 14.0f);
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (face, 13.0f);
            g.reduceClipRegion (clip);
            g.setOpacity (1.0f);
            g.drawImageAt (brushed, 0, 0);
            // a diagonal sheen across the metal
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.0f), face.getX(), face.getY(),
                                                     juce::Colours::white.withAlpha (0.0f), face.getRight(), face.getBottom(), false));
            juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), face.getX(), face.getY() + face.getHeight() * 0.1f,
                                        juce::Colours::white.withAlpha (0.0f), face.getRight(), face.getBottom() - face.getHeight() * 0.1f, false);
            sheen.addColour (0.38, juce::Colours::white.withAlpha (0.0f));
            sheen.addColour (0.46, juce::Colours::white.withAlpha (0.16f));
            sheen.addColour (0.56, juce::Colours::white.withAlpha (0.0f));
            g.setGradientFill (sheen);
            g.fillRect (face);
            // darker toward the bottom: the light comes from above
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0.0f, face.getCentreY(),
                                                     juce::Colours::black.withAlpha (0.14f), 0.0f, face.getBottom(), false));
            g.fillRect (face);
        }
        // bevelled edge: bright on top, shaded underneath
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.85f), 0.0f, face.getY(),
                                                 juce::Colours::black.withAlpha (0.35f), 0.0f, face.getBottom(), false));
        g.drawRoundedRectangle (face.reduced (1.0f), 12.0f, 1.8f);
        g.setColour (juce::Colour (0xff2c2f33));
        g.drawRoundedRectangle (face, 13.0f, 1.0f);

        // four screws holding the plate on
        const float inset = 10.0f;
        for (auto p : { juce::Point<float> (face.getX() + inset, face.getY() + inset), juce::Point<float> (face.getRight() - inset, face.getY() + inset),
                        juce::Point<float> (face.getX() + inset, face.getBottom() - inset), juce::Point<float> (face.getRight() - inset, face.getBottom() - inset) })
            drawScrew (g, p, 3.4f);
    }

    void styleAmpButton (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, Palette::plateLight);
        b.setColour (juce::TextButton::buttonOnColourId, Palette::amber);
        b.setColour (juce::TextButton::textColourOffId, Palette::cream);
        b.setColour (juce::TextButton::textColourOnId, Palette::inkDark);
        b.setColour (juce::ComboBox::outlineColourId, Palette::cream);
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
    const int top = juce::jmax (0, (getHeight() - 340) / 2);

    const juce::String noteText = haveNote ? juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1) : "--";
    g.setColour (noteColour);
    g.setFont (juce::FontOptions (96.0f, juce::Font::bold));
    g.drawText (noteText, juce::Rectangle<int> (0, top + 10, getWidth(), 110), juce::Justification::centred);

    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText (haveNote ? juce::String (hz, 1) + " Hz" : "Play a string", juce::Rectangle<int> (0, top + 126, getWidth(), 22),
                juce::Justification::centred);

    // meter: -50..+50 cents
    const float halfWidth = std::min (320.0f, b.getWidth() * 0.5f - 30.0f);
    const float cx = b.getCentreX(), meterY = (float) top + 172.0f;
    g.setColour (juce::Colour (0xff1e1e21));
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
                    juce::Rectangle<int> (0, top + 226, getWidth(), 56), juce::Justification::centred);

        g.setColour (noteColour);
        g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        g.drawText (inTune ? "IN TUNE" : displayCents < 0.0f ? "FLAT - tune up" : "SHARP - tune down",
                    juce::Rectangle<int> (0, top + 284, getWidth(), 24), juce::Justification::centred);
    }

    g.setColour (Palette::dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Output is muted while the tuner is open. Click the tuning fork again, or any tab, to go back",
                juce::Rectangle<int> (0, top + 318, getWidth(), 20), juce::Justification::centred);
}

// ---------------------------------------------------------------- amp view

namespace
{
    struct HeadLook { juce::Colour tolex, grille, panel, jewel; };
    HeadLook headLook (int head)
    {
        switch (head)
        {
            // all black with metallic cloth, from bright silver to gunmetal to charcoal
            case 0:  return { juce::Colour (0xff17171a), juce::Colour (0xffb7bcc2), juce::Colour (0xff0c0c0d), juce::Colour (0xffff3b30) };
            case 1:  return { juce::Colour (0xff131315), juce::Colour (0xff7c838b), juce::Colour (0xff0b0b0c), juce::Colour (0xff4fd16a) };
            default: return { juce::Colour (0xff101012), juce::Colour (0xff46484e), juce::Colour (0xff0d0d0f), juce::Colour (0xff42a9ff) };
        }
    }
}

AmpView::AmpView (GuitarDriveProcessor& p, const juce::Image& logoImage)
    : proc (p), apvts (p.apvts), logo (logoImage)
{
    const auto theme = Themes::amp();
    const auto& defs = Params::stripKnobs();   // gate, trans, dwell, tone, mix
    for (size_t k = 0; k < unitKnobs.size(); ++k)
        unitKnobs[k].init (*this, apvts, defs[k].id, defs[k].label, theme, true);
    unitKnobs[1].slider.setWheelFractions (1.0 / 24.0, 1.0 / 24.0);   // one notch per semitone
    unitKnobs[2].slider.setWheelFractions (1.0 / 9.0, 1.0 / 9.0);     // the tank's ten positions
    unitKnobs[3].slider.setWheelFractions (1.0 / 9.0, 1.0 / 9.0);
    unitKnobs[2].slider.setTooltip ("DWELL: how hard the spring tank is driven. Higher is longer and splashier.");
    unitKnobs[3].slider.setTooltip ("TONE: darker to brighter reverb.");
    unitKnobs[4].slider.setTooltip ("MIX: how much reverb goes into the amps.");

    for (int h = 0; h < Params::numHeads; ++h)
    {
        auto& head = heads[(size_t) h];
        for (const auto& k : Params::headKnobs (h))
        {
            head.knobs.push_back (std::make_unique<KnobUI>());
            head.knobs.back()->init (*this, apvts, Params::headId (h, k.id), k.label, Themes::neutral(), true);   // chrome on the black heads
        }
        head.power = std::make_unique<JewelSwitch> (headLook (h).jewel);
        head.power->setTooltip (juce::String (Params::headTitle (h)) + " on / off");
        addAndMakeVisible (*head.power);
        head.powerAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, Params::headId (h, "on"), *head.power);
        head.knobs.back()->slider.setTooltip ("Cabinet microphones: 0 dark, 5 balanced, 10 clean and bright.");
    }

    // the Dumble's VOICE knob walks through the captures: show which one, by name
    auto& voice = heads[2].knobs.front()->slider;
    voice.setTooltip ("VOICE: the Dumble captures, from the cleanest (KLEAN) to the most driven.");
    voice.onValueChange = [this] { repaint (heads[2].display.expanded (4)); };
    if (! proc.hasDumble())
        for (auto& k : heads[2].knobs)
            k->slider.setEnabled (false);
}

void AmpView::resized()
{
    auto area = getLocalBounds();

    // spring reverb unit, centred along the top, with the gate and transposer in a small box to its left
    unit = juce::Rectangle<int> (0, 2, 700, 134).withCentre ({ area.getCentreX() + 70, 2 + 67 });
    unitGrille = unit.reduced (12, 12).removeFromLeft (230);
    unitPanel = unit.reduced (12, 12).withTrimmedLeft (242);
    utilityPanel = juce::Rectangle<int> (unit.getX() - 200, unit.getY() + 10, 184, unit.getHeight() - 20);
    {
        auto row = utilityPanel.reduced (4, 2).withTrimmedTop (12);
        unitKnobs[0].place (row.removeFromLeft (row.getWidth() / 2), 16);
        unitKnobs[1].place (row, 16);
    }
    {
        auto row = unitPanel.reduced (10, 2).withTrimmedTop (14);
        const int col = row.getWidth() / 3;
        for (int k = 2; k < 5; ++k)
            unitKnobs[(size_t) k].place (row.removeFromLeft (col).withSizeKeepingCentre (juce::jmin (col, 96), row.getHeight()), 16);
    }

    // three heads side by side below it
    auto row = area.withTrimmedTop (unit.getBottom() + 12).reduced (18, 0).withTrimmedBottom (6);
    const int space = 14;
    const int w = (row.getWidth() - 2 * space) / Params::numHeads;
    for (int h = 0; h < Params::numHeads; ++h)
    {
        auto& head = heads[(size_t) h];
        head.body = row.removeFromLeft (w);
        row.removeFromLeft (space);

        auto inner = head.body.reduced (12, 10);
        head.grille = inner.removeFromTop (juce::jmax (40, inner.getHeight() - 172));
        inner.removeFromTop (8);
        head.panel = inner;

        auto controls = head.panel.reduced (6, 6);
        auto top = controls.removeFromTop (24);
        head.power->setBounds (top.removeFromRight (34).withSizeKeepingCentre (30, 30));
        head.display = top.reduced (4, 2);

        const int count = (int) head.knobs.size();
        const int col = controls.getWidth() / count;
        for (auto& k : head.knobs)
            k->place (controls.removeFromLeft (col), 16);
    }
}

void AmpView::paint (juce::Graphics& g)
{
    paintReverbUnit (g);
    for (int h = 0; h < Params::numHeads; ++h)
        paintHead (g, h);
}

void AmpView::paintReverbUnit (juce::Graphics& g)
{
    // utility box: gate and transposer
    {
        const auto r = utilityPanel.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 8.0f);
        g.setGradientFill (juce::ColourGradient (Palette::plateLight, r.getX(), r.getY(), Palette::plate, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Palette::brass.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Palette::cream.withAlpha (0.8f));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("INPUT", r.withHeight (16.0f).toNearestInt(), juce::Justification::centred);
    }

    const auto box = unit.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (box.translated (0.0f, 4.0f), 10.0f);
    if (unitTweed.isNull() || unitTweed.getWidth() != unit.getWidth() || unitTweed.getHeight() != unit.getHeight())
        unitTweed = Textures::tweed (unit.getWidth(), unit.getHeight());
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (box, 10.0f);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);
        g.drawImageAt (unitTweed, unit.getX(), unit.getY());
    }
    g.setColour (juce::Colour (0xff3b2412).withAlpha (0.8f));
    g.drawRoundedRectangle (box.reduced (0.5f), 10.0f, 2.0f);

    // grille cloth with the maker's badge
    if (unitGrilleImage.isNull() || unitGrilleImage.getWidth() != unitGrille.getWidth())
        unitGrilleImage = Textures::grille (unitGrille.getWidth(), unitGrille.getHeight());
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (unitGrille.toFloat(), 10.0f);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);
        g.drawImageAt (unitGrilleImage, unitGrille.getX(), unitGrille.getY());
    }
    g.setColour (juce::Colour (0xff3b2412).withAlpha (0.85f));
    g.drawRoundedRectangle (unitGrille.toFloat(), 10.0f, 2.0f);
    if (logo.isValid())
    {
        const auto badge = unitGrille.toFloat().reduced (40.0f, 14.0f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawImage (logo, badge.translated (2.0f, 3.0f), juce::RectanglePlacement::centred, true);
        g.setOpacity (1.0f);
        g.drawImage (logo, badge, juce::RectanglePlacement::centred);
    }

    // control plate
    const auto pl = unitPanel.toFloat();
    g.setGradientFill (juce::ColourGradient (Palette::plateLight, pl.getX(), pl.getY(), Palette::plate, pl.getX(), pl.getBottom(), false));
    g.fillRoundedRectangle (pl, 6.0f);
    g.setColour (juce::Colour (0xffb9bcc2));
    g.drawRoundedRectangle (pl.reduced (0.5f), 6.0f, 1.2f);
    g.setColour (Palette::cream);
    g.setFont (scriptFont (16.0f));
    g.drawText ("Reverb", pl.withHeight (18.0f).toNearestInt().translated (0, 1), juce::Justification::centred);
}

void AmpView::paintHead (juce::Graphics& g, int index)
{
    auto& head = heads[(size_t) index];
    const auto look = headLook (index);
    const auto body = head.body.toFloat();
    const bool on = head.power->getToggleState() && (index < 2 || proc.hasDumble());

    // body covered in tolex, with a handle on top
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (body.translated (0.0f, 5.0f), 12.0f);
    if (head.tolexImage.isNull() || head.tolexImage.getWidth() != head.body.getWidth() || head.tolexImage.getHeight() != head.body.getHeight())
        head.tolexImage = Textures::tolex (head.body.getWidth(), head.body.getHeight(), look.tolex, 11 + index);
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (body, 12.0f);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);
        g.drawImageAt (head.tolexImage, head.body.getX(), head.body.getY());
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), 0.0f, body.getY(), juce::Colours::transparentWhite, 0.0f, body.getY() + 16.0f, false));
        g.fillRect (body.withHeight (16.0f));
    }
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (body.reduced (0.5f), 12.0f, 1.2f);

    // grille cloth, tinted for each amp, with its name in script
    const auto gr = head.grille.toFloat();
    if (head.grilleImage.isNull() || head.grilleImage.getWidth() != head.grille.getWidth() || head.grilleImage.getHeight() != head.grille.getHeight())
        head.grilleImage = Textures::grille (head.grille.getWidth(), head.grille.getHeight());
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (gr, 8.0f);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);
        g.drawImageAt (head.grilleImage, head.grille.getX(), head.grille.getY());
        g.setColour (look.grille.withAlpha (0.72f));
        g.fillRect (gr);
        g.setColour (juce::Colours::black.withAlpha (on ? 0.0f : 0.25f));
        g.fillRect (gr);
    }
    // chrome piping around the cloth
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f3f5), 0.0f, gr.getY(), juce::Colour (0xff5d6168), 0.0f, gr.getBottom(), false));
    g.drawRoundedRectangle (gr.expanded (1.5f), 9.0f, 2.2f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (gr, 8.0f, 1.0f);

    const juce::String title (Params::headTitle (index));
    const auto titleColour = look.grille.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff1c1c1e) : juce::Colour (0xffe9ebee);
    g.setFont (scriptFont (juce::jmin (46.0f, gr.getHeight() * 0.62f)));
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawText (title, gr.translated (2.0f, 3.0f).toNearestInt(), juce::Justification::centred);
    g.setColour (titleColour);
    g.drawText (title, gr.toNearestInt(), juce::Justification::centred);

    // control panel
    const auto pl = head.panel.toFloat();
    g.setGradientFill (juce::ColourGradient (look.panel.brighter (0.15f), pl.getX(), pl.getY(), look.panel, pl.getX(), pl.getBottom(), false));
    g.fillRoundedRectangle (pl, 6.0f);
    g.setColour (juce::Colour (0xffb9bcc2));
    g.drawRoundedRectangle (pl.reduced (0.5f), 6.0f, 1.2f);
    for (auto corner : { pl.getTopLeft() + juce::Point<float> (8.0f, 8.0f), pl.getBottomLeft() + juce::Point<float> (8.0f, -8.0f) })
        drawScrew (g, corner, 2.6f);

    // what the head is playing: the amp's circuit, or the Dumble capture the VOICE knob points at
    const auto d = head.display.toFloat();
    g.setColour (juce::Colour (0xff060807));
    g.fillRoundedRectangle (d, 4.0f);
    g.setColour (Palette::brass.withAlpha (0.5f));
    g.drawRoundedRectangle (d, 4.0f, 1.0f);
    juce::String info;
    if (index == 0)
        info = "TWIN REVERB  -  CIRCUIT MODEL";
    else if (index == 1)
        info = "CLASSIC CLEAN  -  CIRCUIT MODEL";
    else
        info = proc.hasDumble() ? "ODS #102  -  " + proc.dumbleVoiceName ((float) heads[2].knobs.front()->slider.getValue())
                                : "NO CAPTURES IN THIS BUILD";
    g.setColour (on ? Palette::amber : Palette::amber.withAlpha (0.35f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (info, d.reduced (8.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, true);
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

    auto theme = Themes::pedal (colours.knob);
    theme.label = theme.scale = colours.knob.darker (0.65f).withMultipliedSaturation (1.6f);   // printed in the pedal's color
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

    drawEnclosure (g, getLocalBounds().toFloat(), brushed, colours.knob.darker (0.25f));

    // the pedal's name in script, in its own color
    g.setColour (colours.knob.darker (0.65f).withMultipliedSaturation (1.6f));
    g.setFont (scriptFont ((float) titleArea.getHeight() * 0.95f));
    g.drawText (titleCase (def.title), titleArea, juce::Justification::centred);

    if (! creditArea.isEmpty())
    {
        g.setColour (Palette::inkDark.withAlpha (0.7f));
        g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
        g.drawText ("DESIGNED BY JOE GARCIA", creditArea, juce::Justification::centred);
    }
}

void PedalPanel::resized()
{
    auto area = pedalFace (getLocalBounds().toFloat()).toNearestInt().reduced (10, 8);
    const bool roomy = area.getHeight() >= 300;

    creditArea = roomy ? area.removeFromBottom (14) : juce::Rectangle<int>();
    footSwitch.setBounds (area.removeFromBottom (roomy ? 76 : 58));
    titleArea = area.removeFromBottom (roomy ? 46 : 32);

    const int n = (int) knobs.size();
    const int cols = n <= 2 ? 1 : 2;
    const int rows = (n + cols - 1) / cols;
    const int cellW = area.getWidth() / cols, cellH = area.getHeight() / rows;

    for (int k = 0; k < n; ++k)
    {
        const int row = k / cols, col = k % cols;
        const int inRow = juce::jmin (cols, n - row * cols);   // a short last row is centred
        const int x0 = area.getX() + (cols - inRow) * cellW / 2 + col * cellW;
        knobs[(size_t) k]->place ({ x0, area.getY() + row * cellH, cellW, cellH }, 15);
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
    constexpr int space = 22;
    int total = space * ((int) panels.size() - 1);
    for (auto& p : panels)
        total += p->preferredWidth();

    const int height = juce::jmin (410, getHeight() - 16);
    auto row = getLocalBounds().withSizeKeepingCentre (total, height);
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
    auto area = pedalFace (getLocalBounds().toFloat()).toNearestInt().reduced (10, 8);
    const bool roomy = area.getHeight() >= 300;
    footSwitch.setBounds (area.removeFromBottom (roomy ? 76 : 58));
    titleArea = area.removeFromBottom (roomy ? 46 : 32);
    treadle.setBounds (area.reduced (4, 2));
}

void WahPedal::paint (juce::Graphics& g)
{
    if (brushed.isNull() || brushed.getWidth() != getWidth() || brushed.getHeight() != getHeight())
        brushed = Textures::brushedAluminum (getWidth(), getHeight(), 4242);

    const juce::Colour purple (0xffd5c3e6);
    drawEnclosure (g, getLocalBounds().toFloat(), brushed, purple.darker (0.25f));
    g.setColour (purple.darker (0.65f).withMultipliedSaturation (1.6f));
    g.setFont (scriptFont ((float) titleArea.getHeight() * 0.95f));
    g.drawText ("Wah", titleArea, juce::Justification::centred);
}

WahView::WahView (juce::AudioProcessorValueTreeState& apvts, int strip)
    : wah (apvts, strip), autoWah (apvts, Params::wahAutoPedal(), strip)
{
    addAndMakeVisible (wah);
    addAndMakeVisible (autoWah);
}

void WahView::resized()
{
    constexpr int wahWidth = 300, space = 26;
    const int total = wahWidth + space + autoWah.preferredWidth();
    auto row = getLocalBounds().withSizeKeepingCentre (total, juce::jmin (410, getHeight() - 16));
    wah.setBounds (row.removeFromLeft (wahWidth));
    row.removeFromLeft (space);
    autoWah.setBounds (row);
}

// ---------------------------------------------------------------- equalizer

EqView::EqView (juce::AudioProcessorValueTreeState& apvts, int strip)
{
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, GuitarDriveProcessor::paramId (strip, Params::eqOn), footSwitch);
    addAndMakeVisible (footSwitch);

    const juce::Colour cap (0xffe9dfc2);   // cream fader caps on a black rack unit
    for (int b = 0; b < Params::numEqBands; ++b)
    {
        auto& band = bands[(size_t) b];
        band.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, GuitarDriveProcessor::paramId (strip, Params::eqId (b)), band.fader);
        band.fader.setDoubleClickReturnValue (true, 0.0);
        band.fader.onValueChange = [this] { updateCurve(); };
        band.fader.setColour (juce::Slider::thumbColourId, cap);
        band.fader.setColour (juce::Slider::backgroundColourId, Palette::dim);
        band.fader.setColour (juce::Slider::textBoxTextColourId, Palette::text);
        band.fader.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (band.fader);

        band.label.setText (juce::String (Params::eqLabel (b)) + " Hz", juce::dontSendNotification);
        band.label.setJustificationType (juce::Justification::centred);
        band.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        band.label.setColour (juce::Label::textColourId, Palette::text);
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
    const auto pedal = getLocalBounds().withSizeKeepingCentre (juce::jmin (1080, getWidth() - 40), juce::jmin (410, getHeight() - 16));
    auto area = pedalFace (pedal.toFloat()).toNearestInt().reduced (58, 14);   // inside the rack ears

    auto top = area.removeFromTop (juce::jmin (110, area.getHeight() / 3));
    auto side = top.removeFromRight (170);
    titleArea = side.removeFromTop (side.getHeight() / 2);
    footSwitch.setBounds (side);
    top.removeFromRight (10);
    curveArea = top;
    area.removeFromTop (8);

    const int colWidth = area.getWidth() / Params::numEqBands;
    for (auto& band : bands)
    {
        auto col = area.removeFromLeft (colWidth);
        band.label.setBounds (col.removeFromTop (18));
        band.fader.setBounds (col.reduced (6, 0));
    }
    updateCurve();
}

void EqView::paint (juce::Graphics& g)
{
    if (brushed.isNull() || brushed.getWidth() != getWidth() || brushed.getHeight() != getHeight())
        brushed = Textures::brushedAluminum (getWidth(), getHeight(), 99);

    const auto pedal = getLocalBounds().withSizeKeepingCentre (juce::jmin (1080, getWidth() - 40), juce::jmin (410, getHeight() - 16));
    const auto body = pedal.toFloat();

    // shadow under the unit
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.07f));
        g.fillRoundedRectangle (body.translated (0.0f, 4.0f + (float) i).expanded ((float) i * 0.8f - 2.0f, 0.0f), 10.0f + (float) i);
    }

    // rack ears: brushed steel with two slotted mounting holes each
    const float earW = 40.0f;
    for (auto ear : { body.withWidth (earW), body.withLeft (body.getRight() - earW) })
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (ear, 8.0f);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);
        g.drawImageAt (brushed, 0, 0);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRect (ear);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.6f), 0.0f, ear.getY(), juce::Colours::black.withAlpha (0.4f), 0.0f, ear.getBottom(), false));
        g.drawRoundedRectangle (ear.reduced (1.0f), 7.0f, 1.5f);
        for (float fy : { 0.18f, 0.82f })
        {
            const auto hole = juce::Rectangle<float> (12.0f, 22.0f).withCentre ({ ear.getCentreX(), ear.getY() + ear.getHeight() * fy });
            g.setColour (juce::Colour (0xff050505));
            g.fillRoundedRectangle (hole, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.drawRoundedRectangle (hole.translated (0.0f, 1.0f), 6.0f, 0.8f);
            drawScrew (g, hole.getCentre(), 5.0f);
        }
    }

    // the chassis between the ears: black anodised, lit along the top
    const auto chassis = body.reduced (earW - 2.0f, 0.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3d40), chassis.getX(), chassis.getY(), juce::Colour (0xff121214), chassis.getX(), chassis.getBottom(), false));
    g.fillRoundedRectangle (chassis, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawHorizontalLine ((int) chassis.getY() + 1, chassis.getX() + 6.0f, chassis.getRight() - 6.0f);

    // recessed front panel with a brass pinstripe
    const auto face = pedalFace (body).reduced (earW - 6.0f, 0.0f);
    g.setColour (juce::Colour (0xff0d0e0f));
    g.fillRoundedRectangle (face, 8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), 0.0f, face.getY(), juce::Colours::white.withAlpha (0.18f), 0.0f, face.getBottom(), false));
    g.drawRoundedRectangle (face, 8.0f, 2.0f);   // inner shadow at the top, a lit lip at the bottom
    g.setColour (Palette::brass.withAlpha (0.55f));
    g.drawRoundedRectangle (face.reduced (6.0f), 6.0f, 1.0f);

    g.setColour (Palette::text);
    g.setFont (scriptFont (juce::jmin (40.0f, (float) titleArea.getHeight() * 0.9f)));
    g.drawText ("Equalizer", titleArea, juce::Justification::centred);

    // response display: a dark green screen behind glass, in a brass bezel
    const auto r = curveArea.toFloat();
    g.setColour (Palette::brassDark);
    g.fillRoundedRectangle (r.expanded (3.0f), 8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff10261b), r.getCentreX(), r.getCentreY(), juce::Colour (0xff030a06), r.getX(), r.getY(), true));
    g.fillRoundedRectangle (r, 6.0f);

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
    g.setColour (Palette::amber.withAlpha (0.25f));   // phosphor glow
    g.strokePath (path, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Palette::amber);
    g.strokePath (path, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), r.getX(), r.getY(),
                                             juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.45f, false));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f), 6.0f);   // glass reflection
}

// ---------------------------------------------------------------- footer: tuner and metronome

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

    slashLabel.setText ("/", juce::dontSendNotification);
    slashLabel.setJustificationType (juce::Justification::centred);
    slashLabel.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    slashLabel.setColour (juce::Label::textColourId, Palette::text);
    addAndMakeVisible (slashLabel);

    clickLabel.setText ("CLICK", juce::dontSendNotification);
    clickLabel.setJustificationType (juce::Justification::centredRight);
    clickLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    clickLabel.setColour (juce::Label::textColourId, Palette::text);
    addAndMakeVisible (clickLabel);

    clickKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    clickKnob.setLook (KnobStyle::ring, juce::Colours::white, Palette::silver);
    clickAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, Params::metroVol, clickKnob);
    addAndMakeVisible (clickKnob);

    bpmBox.onUserValue = [this] (int v) { bpmAttach.setValueAsCompleteGesture ((float) v); };
    beatsBox.onUserValue = [this] (int v) { beatsAttach.setValueAsCompleteGesture ((float) v); };
    noteBox.onUserValue = [this] (int v)
    {
        const int index = Metronome::noteIndexFor (v);   // only 2, 4, 8 and 16 are note values
        noteAttach.setValueAsCompleteGesture ((float) index);
        noteBox.showNumber (Metronome::noteValue (index));
    };
    for (auto* box : { &bpmBox, &beatsBox, &noteBox })
    {
        box->setColour (juce::TextEditor::backgroundColourId, juce::Colours::black);
        box->setColour (juce::TextEditor::outlineColourId, juce::Colours::white.withAlpha (0.75f));
        box->applyFontToAllText (juce::FontOptions (18.0f, juce::Font::bold));
        addAndMakeVisible (*box);
    }

    tapButton.setColour (juce::TextButton::buttonColourId, Palette::feltDeep);
    tapButton.setColour (juce::ComboBox::outlineColourId, Palette::brass);
    tapButton.onClick = [this] { tap(); };
    addAndMakeVisible (tapButton);

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
    auto area = getLocalBounds().reduced (22, 8);
    const auto centred = [&area] (int width, int height) { return area.removeFromLeft (width).withSizeKeepingCentre (width, height); };

    tunerButton.setBounds (centred (44, 44));
    area.removeFromLeft (14);
    playButton.setBounds (centred (44, 44));
    area.removeFromLeft (16);
    tapButton.setBounds (centred (58, 32));
    bpmBox.setBounds (centred (72, 32));
    area.removeFromLeft (22);
    beatsBox.setBounds (centred (48, 32));
    slashLabel.setBounds (centred (22, 32));
    noteBox.setBounds (centred (48, 32));
    area.removeFromLeft (24);

    clickKnob.setBounds (area.removeFromRight (46).withSizeKeepingCentre (46, 46));
    clickLabel.setBounds (area.removeFromRight (60));
    area.removeFromRight (12);
    dotsArea = area.withSizeKeepingCentre (area.getWidth(), 24);
}

void FooterBar::paint (juce::Graphics& g)
{
    // one light per pulse of the bar; the strong pulse is amber, the medium ones blue
    const int beats = Metronome::clampBeats ((int) std::lround (proc.apvts.getRawParameterValue (Params::metroBeats)->load()));
    const int note = Metronome::noteValue ((int) std::lround (proc.apvts.getRawParameterValue (Params::metroNote)->load()));
    const int current = proc.getMetronome().getPulse();
    constexpr float d = 14.0f, space = 7.0f;
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
            g.setColour (lit);
            g.fillEllipse (dot);
        }
        g.setColour (i == current ? lit : lit.withAlpha (accent == 2 ? 0.9f : 0.45f));
        g.drawEllipse (dot.reduced (0.75f), 1.5f);
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

    nameButton.setColour (juce::TextButton::buttonColourId, Palette::feltDeep);
    nameButton.setColour (juce::ComboBox::outlineColourId, Palette::brass);
    nameButton.setColour (juce::TextButton::textColourOffId, Palette::text);
    nameButton.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);

    for (juce::Button* b : std::initializer_list<juce::Button*> { &prevButton, &nextButton, &nameButton, &saveButton, &newButton, &deleteButton })
        addAndMakeVisible (*b);

    refresh();
    startTimerHz (4);
}

void PresetBar::resized()
{
    const int cx = getWidth() / 2;
    saveButton.setBounds (cx - 56 - 17, 0, 34, 30);
    newButton.setBounds (cx - 17, 0, 34, 30);
    deleteButton.setBounds (cx + 56 - 17, 0, 34, 30);

    auto row = juce::Rectangle<int> (0, 42, getWidth(), 36);
    prevButton.setBounds (row.removeFromLeft (30));
    nextButton.setBounds (row.removeFromRight (30));
    nameButton.setBounds (row.reduced (8, 0));
}

void PresetBar::paint (juce::Graphics& g)
{
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("PRESETS", juce::Rectangle<int> (0, 100, getWidth(), 22), juce::Justification::centred);

    // menu arrow at the right end of the name
    const auto r = nameButton.getBounds().toFloat();
    juce::Path arrow;
    arrow.addTriangle (r.getRight() - 24.0f, r.getCentreY() - 3.0f, r.getRight() - 14.0f, r.getCentreY() - 3.0f, r.getRight() - 19.0f, r.getCentreY() + 4.0f);
    g.setColour (Palette::text);
    g.fillPath (arrow);
}

void PresetBar::refresh()
{
    auto& presets = proc.presets;
    const auto name = presets.currentName();
    const bool dirty = presets.isDirty();

    const juce::String text = name.isEmpty() ? juce::String ("No preset") : name + (dirty ? " *" : "");
    if (text != shownText)
    {
        shownText = text;
        nameButton.setButtonText (text);
    }
    saveButton.setEnabled (name.isEmpty() || dirty);   // with no preset selected, save creates one
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
      inputMeter (p.inputPeak),
      outputMeter (p.outputPeak),
      ampView (p, logoImage),
      footer (p),
      presetBar (p),
      tunerView (p)
{
    // a clean geometric sans for the whole interface, when the machine has one
    for (const char* name : { "Century Gothic", "Bahnschrift" })
        if (fontInstalled (name))
        {
            juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName (name);
            break;
        }

    setLookAndFeel (&look);
    setResizable (true, false);
    addAndMakeVisible (root);
    root.painter = [this] (juce::Graphics& g) { paintRoot (g); };

    const std::pair<Icon, const char*> tabDefs[5] = { { Icon::wah, "WAH" }, { Icon::pedals, "PRE FX" }, { Icon::amp, "AMP" },
                                                      { Icon::eq, "EQ" }, { Icon::post, "POST FX" } };
    for (size_t i = 0; i < tabs.size(); ++i)
    {
        tabs[i] = std::make_unique<IconButton> (tabDefs[i].first, tabDefs[i].second);
        tabs[i]->setClickingTogglesState (true);
        tabs[i]->setRadioGroupId (101);
        tabs[i]->onClick = [this, i] { setView ((View) i); };
        root.addAndMakeVisible (*tabs[i]);
    }
    tabs[(size_t) View::amp]->setToggleState (true, juce::dontSendNotification);

    inputKnob.init (root, p.apvts, Params::inputKnob().id, Params::inputKnob().label);
    outputKnob.init (root, p.apvts, Params::outputKnob().id, Params::outputKnob().label);
    root.addAndMakeVisible (inputMeter);
    root.addAndMakeVisible (outputMeter);

    root.addAndMakeVisible (ampView);
    wahView = std::make_unique<WahView> (p.apvts, 0);
    preBoard = std::make_unique<PedalBoard> (p.apvts, p.hasTs9() ? Params::prePedalsTs9() : Params::prePedals(), 0);
    eqView = std::make_unique<EqView> (p.apvts, 0);
    postBoard = std::make_unique<PedalBoard> (p.apvts, Params::postPedals(), 0);
    for (juce::Component* c : { (juce::Component*) wahView.get(), (juce::Component*) preBoard.get(),
                                (juce::Component*) eqView.get(), (juce::Component*) postBoard.get() })
        root.addAndMakeVisible (c);
    root.addAndMakeVisible (footer);
    root.addAndMakeVisible (presetBar);
    root.addAndMakeVisible (tunerView);
    settingsButton.setTooltip ("Settings and keyboard shortcuts");
    settingsButton.onClick = [this] { showSettingsMenu(); };
    root.addAndMakeVisible (settingsButton);   // last, so it sits on top of the footer

    // Keyboard shortcuts go to the editor: buttons and knobs must not take the keyboard away (Space would press them).
    std::function<void (juce::Component&)> noFocus = [&noFocus] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (dynamic_cast<juce::TextEditor*> (child) == nullptr)
                child->setWantsKeyboardFocus (false);
            noFocus (*child);
        }
    };
    noFocus (root);
    setWantsKeyboardFocus (true);
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<GuitarDriveEditor> (this)]
    {
        if (safe != nullptr && safe->isShowing())
            safe->grabKeyboardFocus();
    });

    footer.tunerButton.onClick = [this] { setTunerOpen (footer.tunerButton.getToggleState()); };

    updateVisibility();
    initialiseSize();
}

GuitarDriveEditor::~GuitarDriveEditor()
{
    proc.setTunerActive (false);   // never leave the output muted behind a closed window
    setLookAndFeel (nullptr);
}

void GuitarDriveEditor::recomputeDesign()
{
    bodyH = monoBodyH;
    designH = topH + bodyH + footerH;
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

void GuitarDriveEditor::initialiseSize()
{
    recomputeDesign();

    // The smallest size is 40% scale: everything is scaled together, so it always fits.
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setResizeLimits (juce::roundToInt ((float) designW * 0.4f), juce::roundToInt ((float) designH * 0.4f),
                     juce::roundToInt ((float) designW * 1.6f), juce::roundToInt ((float) designH * 1.6f));

    const float scale = fitScale();
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

void GuitarDriveEditor::updateVisibility()
{
    const bool normal = ! tunerOpen;
    wahView->setVisible (normal && view == View::wah);
    preBoard->setVisible (normal && view == View::pre);
    eqView->setVisible (normal && view == View::eq);
    postBoard->setVisible (normal && view == View::post);
    ampView.setVisible (normal && view == View::amp);
    tunerView.setVisible (tunerOpen);
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
    bodyRect = { 0, topH, w, bodyH };
    footerRect = { 0, bodyRect.getBottom(), w, footerH };

    // icon tabs along the top, centred
    constexpr int tabW = 70, tabH = 52;
    const int tabsX = (w - tabW * (int) tabs.size()) / 2;
    for (size_t i = 0; i < tabs.size(); ++i)
        tabs[i]->setBounds (tabsX + (int) i * tabW, 8, tabW, tabH);

    // control row
    inputMeter.setBounds (36, 78, 8, 128);
    inputKnob.place ({ 70, 72, 120, 140 });
    presetBar.setBounds ((w - 420) / 2, 70, 420, 130);
    settingsButton.setBounds (designW - 196, designH - footerH + 16, 30, 30);

    outputKnob.place ({ w - 190, 72, 120, 140 });
    outputMeter.setBounds (w - 44, 78, 8, 128);

    ampView.setBounds (bodyRect);
    for (juce::Component* c : { (juce::Component*) wahView.get(), (juce::Component*) preBoard.get(),
                                (juce::Component*) eqView.get(), (juce::Component*) postBoard.get() })
        c->setBounds (bodyRect);
    footer.setBounds (footerRect);
    tunerView.setBounds (bodyRect);
    repaint();
}

void GuitarDriveEditor::paintRoot (juce::Graphics& g)
{
    if (backdrop.isNull() || backdrop.getWidth() != designW || backdrop.getHeight() != designH)
        backdrop = Textures::felt (designW, designH);
    g.setOpacity (1.0f);
    g.drawImageAt (backdrop, 0, 0);

    // a double cream pinstripe around the edge, with art-deco corners
    {
        const auto outer = juce::Rectangle<float> (0.0f, 0.0f, (float) designW, (float) designH).reduced (7.0f);
        g.setColour (Palette::text.withAlpha (0.45f));
        g.drawRoundedRectangle (outer, 10.0f, 1.2f);
        g.setColour (Palette::brass.withAlpha (0.35f));
        g.drawRoundedRectangle (outer.reduced (4.0f), 7.0f, 0.8f);
        for (auto corner : { outer.getTopLeft(), outer.getTopRight(), outer.getBottomLeft(), outer.getBottomRight() })
        {
            const float sx = corner.x < (float) designW * 0.5f ? 1.0f : -1.0f;
            const float sy = corner.y < (float) designH * 0.5f ? 1.0f : -1.0f;
            const auto c = corner + juce::Point<float> (sx * 16.0f, sy * 16.0f);
            juce::Path diamond;
            diamond.addPolygon (c, 4, 5.0f, juce::MathConstants<float>::pi * 0.25f);
            g.setColour (Palette::brass.withAlpha (0.8f));
            g.fillPath (diamond);
            g.setColour (Palette::text.withAlpha (0.35f));
            g.drawLine (c.x + sx * 9.0f, c.y, c.x + sx * 38.0f, c.y, 1.0f);
            g.drawLine (c.x, c.y + sy * 9.0f, c.x, c.y + sy * 38.0f, 1.0f);
        }
    }

    // pinstripes under the top bar and above the footer, each with a small brass diamond in the middle
    for (float y : { (float) topH - 4.0f, (float) (designH - footerH) + 2.0f })
    {
        juce::ColourGradient line (Palette::text.withAlpha (0.0f), 30.0f, y, Palette::text.withAlpha (0.0f), (float) designW - 30.0f, y, false);
        line.addColour (0.2, Palette::text.withAlpha (0.4f));
        line.addColour (0.8, Palette::text.withAlpha (0.4f));
        g.setGradientFill (line);
        g.fillRect (30.0f, y - 0.5f, (float) designW - 60.0f, 1.0f);
        juce::Path diamond;
        diamond.addPolygon ({ (float) designW * 0.5f, y }, 4, 4.5f, juce::MathConstants<float>::pi * 0.25f);
        g.setColour (Palette::brass);
        g.fillPath (diamond);
    }

    // the artist logo, top right
    if (logoImage.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (logoImage, juce::Rectangle<float> ((float) designW - 150.0f, 4.0f, 126.0f, 60.0f), juce::RectanglePlacement::centred);
    }

}

// ---------------------------------------------------------------- keyboard and settings

namespace
{
    struct Shortcut { char key; const char* param; const char* what; };
    const Shortcut shortcuts[] = {
        { 'q', "wah_on", "Wah" }, { 'w', "comp_on", "Compressor" }, { 'e', "ts_on", "Tube Screamer" }, { 'r', "od_on", "Overdrive / TS9" },
        { 'a', "ph_on", "Phaser" }, { 's', "ch_on", "Chorus" }, { 'd', "tr_on", "Tremolo" }, { 'f', "dl_on", "Delay" }, { 'g', "rv_on", "Digital reverb" },
        { '1', "fn_on", "Fender head" }, { '2', "rk_on", "Two-Rock head" }, { '3', "dm_on", "Dumble head" },
    };
}

void GuitarDriveEditor::toggleParameter (const juce::String& id)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
}

bool GuitarDriveEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())
        return false;   // leave shortcuts like Ctrl+S to the host

    if (key == juce::KeyPress::leftKey)   { presetBar.step (-1); return true; }
    if (key == juce::KeyPress::rightKey)  { presetBar.step (1); return true; }
    if (key == juce::KeyPress::spaceKey)  { toggleParameter (Params::metroOn); return true; }

    const auto c = (char) juce::CharacterFunctions::toLowerCase (key.getTextCharacter());
    if (c == 't')
    {
        setTunerOpen (! tunerOpen);
        return true;
    }
    for (const auto& s : shortcuts)
        if (s.key == c)
        {
            toggleParameter (s.param);
            return true;
        }
    return false;
}

void GuitarDriveEditor::showSettingsMenu()
{
    juce::PopupMenu keys;
    for (const auto& s : shortcuts)
        keys.addItem (juce::String::charToString ((juce::juce_wchar) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) s.key))
                      + "    " + s.what + " on / off", false, false, nullptr);
    keys.addSeparator();
    keys.addItem ("Left / Right    previous / next preset", false, false, nullptr);
    keys.addItem ("T    tuner", false, false, nullptr);
    keys.addItem ("Space    metronome start / stop", false, false, nullptr);

    const auto* autoLevel = proc.apvts.getRawParameterValue (Params::autoLevel);
    juce::PopupMenu menu;
    menu.addSectionHeader ("Settings");
    menu.addItem ("Auto level when stacking pedals (pedals push at most 6 dB louder)", true, autoLevel->load() > 0.5f,
                  [this] { toggleParameter (Params::autoLevel); });
    menu.addSubMenu ("Keyboard shortcuts", keys);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (settingsButton),
                        [safe = juce::Component::SafePointer<GuitarDriveEditor> (this)] (int)
                        {
                            if (safe != nullptr)
                                safe->grabKeyboardFocus();   // keys keep working after the menu closes
                        });
}
