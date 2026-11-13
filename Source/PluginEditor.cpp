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
            w = w.substring (0, 1).toUpperCase() + w.substring (1);
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

AmpView::AmpView (GuitarDriveProcessor& p, const juce::Image& logoImage)
    : apvts (p.apvts), logo (logoImage)
{
    const auto& defs = Params::stripKnobs();
    const auto theme = Themes::amp();

    for (int s = 0; s < 2; ++s)
    {
        auto& strip = strips[(size_t) s];

        for (size_t k = 0; k < defs.size(); ++k)
            strip.knobs[k].init (*this, apvts, GuitarDriveProcessor::paramId (s, defs[k].id), defs[k].label, theme, true);

        // transpose moves in whole semitones: one wheel notch is one semitone
        strip.knobs[1].slider.setWheelFractions (1.0 / 24.0, 1.0 / 24.0);
        // the spring tank's DWELL and TONE have ten positions each: one notch per position
        strip.knobs[10].slider.setWheelFractions (1.0 / 9.0, 1.0 / 9.0);
        strip.knobs[11].slider.setWheelFractions (1.0 / 9.0, 1.0 / 9.0);
        strip.knobs[9].slider.setTooltip ("Cabinet microphone: 0 dark, 5 balanced, 10 clean and bright. Blends the three recorded mics.");
        strip.knobs[10].slider.setTooltip ("Spring tank DWELL: how hard the tank is driven. Higher is longer and splashier.");
        strip.knobs[11].slider.setTooltip ("Spring tank TONE: darker to brighter reverb.");

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

        // amp model: Fender (AB763 Deluxe Reverb, breaks up early) or Two-Rock (big, stiff, clean)
        styleAmpButton (strip.ampType);
        strip.ampType.setTooltip ("FENDER: Deluxe Reverb with its 1x12 cabinet, breaks up around volume 5. TWO-ROCK: big clean amp with its 2x12 cabinet, stays clean much louder.");
        addAndMakeVisible (strip.ampType);
        ampAttach[(size_t) s] = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (GuitarDriveProcessor::paramId (s, GuitarDriveProcessor::ampTypeId)),
            [this, s] (float v)
            {
                auto& st = strips[(size_t) s];
                st.ampState = v > 0.5f ? 1 : 0;
                st.ampType.setButtonText (st.ampState != 0 ? "TWO-ROCK" : "FENDER");
                st.ampType.setToggleState (st.ampState != 0, juce::dontSendNotification);
            });
        strip.ampType.onClick = [this, s] { ampAttach[(size_t) s]->setValueAsCompleteGesture (strips[(size_t) s].ampState != 0 ? 0.0f : 1.0f); };
    }

    for (auto& a : revAttach)
        a->sendInitialUpdate();
    for (auto& a : ampAttach)
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
        const bool shown = s == 0 || stereo;
        for (auto& k : strips[(size_t) s].knobs)
            k.show (shown);
        strips[(size_t) s].revType.setVisible (shown);
        strips[(size_t) s].ampType.setVisible (shown);
    }
}

void AmpView::layoutAmp (int index, juce::Rectangle<int> area)
{
    auto& l = lay[(size_t) index];
    auto& strip = strips[(size_t) index];

    l.cabinet = area.withTrimmedTop (30);   // the handle sits in the strip above
    l.panel = { l.cabinet.getX() + 22, l.cabinet.getY() + 18, l.cabinet.getWidth() - 44, 150 };
    const int grilleTop = l.panel.getBottom() + 16;
    const int grilleHeight = l.cabinet.getBottom() - 14 - grilleTop;
    l.grille = grilleHeight >= 30 ? juce::Rectangle<int> (l.cabinet.getX() + 38, grilleTop, l.cabinet.getWidth() - 76, grilleHeight)
                                  : juce::Rectangle<int>();

    constexpr int col = 86, space = 6;
    auto row = l.panel.reduced (8, 8);
    l.groups.clear();

    const auto take = [&] (const juce::String& title, int width)
    {
        auto r = row.removeFromLeft (width);
        row.removeFromLeft (space);
        l.groups.push_back ({ title, r });
        return r;
    };
    const auto placeKnobs = [&] (juce::Rectangle<int> group, std::initializer_list<int> which)
    {
        group.removeFromTop (14);
        for (int k : which)
            strip.knobs[(size_t) k].place (group.removeFromLeft (col), 16);
        return group;   // whatever is left of the group
    };

    l.jack = take (stereo ? (index == 0 ? "LEFT" : "RIGHT") : "INPUT", 84);
    strip.ampType.setBounds (l.jack.removeFromBottom (38).withSizeKeepingCentre (76, 26));
    placeKnobs (take ("PREAMP", 4 * col), { 0, 1, 2, 3 });
    placeKnobs (take ("TONE", 3 * col), { 4, 5, 6 });
    placeKnobs (take ("CABINET", col), { 9 });
    const auto beside = placeKnobs (take ("REVERB", 3 * col + 84), { 7, 10, 11 });
    strip.revType.setBounds (beside.withSizeKeepingCentre (76, 28));
    placeKnobs (take ("MASTER", col), { 8 });

    l.badge = row;   // the rest: logo (when there is room) and pilot light
    l.groups.push_back ({ {}, row });
}

void AmpView::resized()
{
    auto area = getLocalBounds();
    if (stereo)
    {
        const int half = area.getHeight() / 2;
        layoutAmp (0, area.removeFromTop (half));
        layoutAmp (1, area);
    }
    else
    {
        layoutAmp (0, area);
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
    const auto& l = lay[(size_t) index];
    const auto cab = l.cabinet.toFloat();
    const float cx = cab.getCentreX();

    // carry handle: chrome end caps and a black strap arching above the cabinet
    for (float side : { -1.0f, 1.0f })
    {
        const auto cap = juce::Rectangle<float> (90.0f, 13.0f).withCentre ({ cx + side * 235.0f, cab.getY() - 4.0f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff1f2f4), cap.getX(), cap.getY(), juce::Colour (0xff8d9198), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.0f);
        drawScrew (g, { cap.getCentreX() + side * 18.0f, cap.getCentreY() }, 2.6f);
    }
    juce::Path strap;
    strap.startNewSubPath (cx - 200.0f, cab.getY() - 5.0f);
    strap.quadraticTo (cx, cab.getY() - 46.0f, cx + 200.0f, cab.getY() - 5.0f);
    g.setColour (juce::Colour (0xff232325));
    g.strokePath (strap, juce::PathStrokeType (14.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.strokePath (strap, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (0.0f, -3.0f));

    // tweed cabinet, edge to edge
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (cab.getX(), cab.getY(), cab.getWidth(), cab.getHeight(), 12.0f, 12.0f, true, true, false, false);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f);   // images are drawn with the current opacity
        g.drawImageAt (tweedImage, 0, 0);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), 0.0f, cab.getY(),
                                                 juce::Colours::transparentWhite, 0.0f, cab.getY() + 14.0f, false));
        g.fillRect (cab.withHeight (14.0f));
    }

    // control panel: black plate with a chrome edge, divided into sections
    const auto pl = l.panel.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (pl.translated (0.0f, 3.0f), 6.0f);
    g.setGradientFill (juce::ColourGradient (Palette::plateLight, pl.getX(), pl.getY(), Palette::plate, pl.getX(), pl.getBottom(), false));
    g.fillRoundedRectangle (pl, 6.0f);
    g.setColour (juce::Colour (0xffb9bcc2));
    g.drawRoundedRectangle (pl.reduced (1.0f), 6.0f, 2.0f);

    const juce::Font titleFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (const auto& group : l.groups)
    {
        const auto r = group.area.toFloat();
        g.setColour (Palette::cream.withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        if (group.title.isNotEmpty())   // the title sits on the top line, which is broken behind it
        {
            const float w = juce::GlyphArrangement::getStringWidth (titleFont, group.title) + 14.0f;
            const auto tag = juce::Rectangle<float> (w, 12.0f).withCentre ({ r.getCentreX(), r.getY() + 1.0f });
            g.setColour (Palette::plateLight);
            g.fillRect (tag);
            g.setColour (Palette::cream);
            g.setFont (titleFont);
            g.drawText (group.title, tag.translated (0.0f, 1.0f).toNearestInt(), juce::Justification::centred);
        }
    }

    // input jack
    const auto jack = l.jack.toFloat().getCentre().translated (0.0f, 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f3f5), jack.x - 7.0f, jack.y - 9.0f,
                                             juce::Colour (0xff6d7077), jack.x + 9.0f, jack.y + 11.0f, true));
    g.fillEllipse (jack.x - 19.0f, jack.y - 19.0f, 38.0f, 38.0f);
    juce::Path nut;
    nut.addPolygon (jack, 6, 14.0f, 0.3f);
    g.setColour (juce::Colour (0xff8c9097));
    g.fillPath (nut);
    g.setColour (juce::Colour (0xff09090a));
    g.fillEllipse (jack.x - 8.0f, jack.y - 8.0f, 16.0f, 16.0f);

    // logo, and the pilot jewel beside it
    const auto badge = l.badge.toFloat();
    const bool roomForLogo = badge.getWidth() > 160.0f;
    if (logo.isValid() && roomForLogo)
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (logo, badge.withTrimmedRight (58.0f).reduced (8.0f, 8.0f), juce::RectanglePlacement::centred);
    }
    const juce::Point<float> lamp (roomForLogo ? badge.getRight() - 32.0f : badge.getCentreX(), badge.getCentreY());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff4a36).withAlpha (0.55f), lamp.x, lamp.y,
                                             juce::Colours::transparentBlack, lamp.x + 34.0f, lamp.y, true));
    g.fillEllipse (lamp.x - 34.0f, lamp.y - 34.0f, 68.0f, 68.0f);
    g.setColour (juce::Colour (0xffb9bcc2));
    g.fillEllipse (lamp.x - 16.0f, lamp.y - 16.0f, 32.0f, 32.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffa58e), lamp.x - 4.0f, lamp.y - 5.0f,
                                             juce::Colour (0xffb01408), lamp.x + 7.0f, lamp.y + 8.0f, true));
    g.fillEllipse (lamp.x - 12.0f, lamp.y - 12.0f, 24.0f, 24.0f);
    g.setColour (juce::Colours::white.withAlpha (0.65f));
    g.fillEllipse (lamp.x - 7.0f, lamp.y - 8.0f, 8.0f, 5.0f);

    // grille cloth
    if (! l.grille.isEmpty())
    {
        const auto gr = l.grille.toFloat();
        if (grilleImage.isNull() || grilleImage.getWidth() != l.grille.getWidth() || grilleImage.getHeight() != l.grille.getHeight())
            grilleImage = Textures::grille (l.grille.getWidth(), l.grille.getHeight());

        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addRoundedRectangle (gr, 26.0f);
            g.reduceClipRegion (clip);
            g.setOpacity (1.0f);
            g.drawImageAt (grilleImage, l.grille.getX(), l.grille.getY());
        }
        g.setColour (juce::Colour (0xff3b2412).withAlpha (0.85f));
        g.drawRoundedRectangle (gr, 26.0f, 3.0f);
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
      tunerView (p),
      stereoAttach (*p.apvts.getParameter (GuitarDriveProcessor::stereoId), [this] (float v) { setStereo (v > 0.5f); }),
      rightInAttach (*p.apvts.getParameter (GuitarDriveProcessor::rightSameId),
                     [this] (float v) { rightUsesIn1 = v > 0.5f; rightInButton.setToggleState (rightUsesIn1, juce::dontSendNotification); }),
      soloAttach (*p.apvts.getParameter (GuitarDriveProcessor::soloId),
                  [this] (float v) { soloState = (int) std::lround (v); updateSoloButtons(); })
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

    stereoSwitch.onClick = [this] { stereoAttach.setValueAsCompleteGesture (stereoMode ? 0.0f : 1.0f); };
    root.addAndMakeVisible (stereoSwitch);

    // pan, solo and mute for both channels sit in the top bar while in stereo mode
    for (int ch = 0; ch < 2; ++ch)
    {
        auto& c = channels[(size_t) ch];
        c.pan.init (root, p.apvts, GuitarDriveProcessor::paramId (ch, GuitarDriveProcessor::panId), "PAN", Themes::ring(), true);

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

void GuitarDriveEditor::setStereo (bool on)
{
    const bool first = ! sizeInitialised;
    const float oldScale = first ? 0.0f : (float) getWidth() / (float) designW;

    stereoMode = on;
    stereoSwitch.setToggleState (on, juce::dontSendNotification);
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
    stereoSwitch.setBounds (250, 86, 130, 58);
    rightInButton.setBounds (238, 194, 150, 20);
    presetBar.setBounds ((w - 420) / 2, 70, 420, 130);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto& c = channels[(size_t) ch];
        const int gx = 925 + ch * 122;
        c.pan.place ({ gx, 76, 58, 86 }, 14);
        c.name.setBounds (gx + 60, 80, 56, 14);
        c.solo.setBounds (gx + 60, 98, 56, 21);
        c.mute.setBounds (gx + 60, 123, 56, 21);
    }

    outputKnob.place ({ w - 190, 72, 120, 140 });
    outputMeter.setBounds (w - 44, 78, 8, 128);

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

    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("INPUT MODE", juce::Rectangle<int> (238, 170, 150, 22), juce::Justification::centred);
    if (stereoMode)
        g.drawText ("CHANNELS", juce::Rectangle<int> (925, 170, 240, 22), juce::Justification::centred);

    const bool pedalView = ! tunerOpen && view != View::amp;
    if (pedalView && stereoMode)   // left channel above, right channel below
    {
        g.setColour (Palette::text.withAlpha (0.25f));
        g.drawHorizontalLine (channelRect (1).getY() - gap / 2, 24.0f, (float) designW - 24.0f);

        g.setColour (Palette::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        for (int ch = 0; ch < 2; ++ch)
            g.drawText (ch == 0 ? "LEFT" : "RIGHT", channelRect (ch).withWidth (sideTagW + 16), juce::Justification::centred);
    }
}
