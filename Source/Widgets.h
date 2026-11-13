#pragma once
#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>
#include "VintageLook.h"

enum class Icon { wah, pedals, amp, eq, post, tuner, save, trash, plus, prev, next, gear };

// Flat line-icon button with an optional small caption underneath. Lit amber while toggled on.
class IconButton : public juce::Button
{
public:
    explicit IconButton (Icon i, const juce::String& captionText = {}) : juce::Button (captionText), icon (i), caption (captionText) {}

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        auto area = getLocalBounds().toFloat();
        const auto colour = ! isEnabled() ? Palette::text.withAlpha (0.22f)
                          : getToggleState() ? Palette::amber
                          : (highlighted || down) ? Palette::text : Palette::text.withAlpha (0.72f);

        if (getToggleState())   // a small pilot lamp under the chosen tab
        {
            const juce::Point<float> lamp (area.getCentreX(), area.getBottom() - 1.5f);
            g.setGradientFill (juce::ColourGradient (Palette::amber.withAlpha (0.55f), lamp.x, lamp.y,
                                                     Palette::amber.withAlpha (0.0f), lamp.x + 10.0f, lamp.y, true));
            g.fillEllipse (lamp.x - 10.0f, lamp.y - 10.0f, 20.0f, 20.0f);
            g.setColour (Palette::amber);
            g.fillEllipse (lamp.x - 2.0f, lamp.y - 2.0f, 4.0f, 4.0f);
            area.removeFromBottom (4.0f);
        }

        if (caption.isNotEmpty())
        {
            g.setColour (colour);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText (caption, area.removeFromBottom (13.0f).toNearestInt(), juce::Justification::centred);
        }

        const float s = juce::jmin (area.getWidth(), area.getHeight()) - 6.0f;
        const auto box = juce::Rectangle<float> (s, s).withCentre (area.getCentre());
        const auto place = juce::AffineTransform::scale (s).translated (box.getX(), box.getY());

        juce::Path lines, solid;
        build (lines, solid);
        g.setColour (colour);
        g.strokePath (lines, juce::PathStrokeType (juce::jmax (1.5f, s * 0.065f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded), place);
        g.fillPath (solid, place);
    }

private:
    void build (juce::Path& p, juce::Path& f) const
    {
        switch (icon)
        {
            case Icon::wah:
                p.addRoundedRectangle (0.28f, 0.05f, 0.44f, 0.9f, 0.08f);
                p.startNewSubPath (0.38f, 0.68f); p.lineTo (0.62f, 0.68f);
                p.startNewSubPath (0.38f, 0.79f); p.lineTo (0.62f, 0.79f);
                break;
            case Icon::pedals:
                p.addRoundedRectangle (0.2f, 0.05f, 0.6f, 0.9f, 0.08f);
                p.startNewSubPath (0.32f, 0.5f); p.lineTo (0.42f, 0.5f); p.lineTo (0.47f, 0.3f);
                p.lineTo (0.53f, 0.7f); p.lineTo (0.58f, 0.5f); p.lineTo (0.68f, 0.5f);
                break;
            case Icon::amp:
                p.addRoundedRectangle (0.02f, 0.22f, 0.96f, 0.56f, 0.06f);
                p.startNewSubPath (0.02f, 0.45f); p.lineTo (0.98f, 0.45f);
                break;
            case Icon::eq:
            {
                const float xs[3] = { 0.2f, 0.5f, 0.8f }, ys[3] = { 0.66f, 0.34f, 0.72f };
                for (int i = 0; i < 3; ++i)
                {
                    p.startNewSubPath (xs[i], 0.05f); p.lineTo (xs[i], 0.95f);
                    f.addEllipse (xs[i] - 0.1f, ys[i] - 0.1f, 0.2f, 0.2f);
                }
                break;
            }
            case Icon::post:
                p.addRoundedRectangle (0.22f, 0.05f, 0.56f, 0.9f, 0.08f);
                p.addTriangle (0.36f, 0.3f, 0.64f, 0.3f, 0.5f, 0.5f);
                p.addTriangle (0.5f, 0.5f, 0.36f, 0.7f, 0.64f, 0.7f);
                break;
            case Icon::tuner:
                p.startNewSubPath (0.3f, 0.05f); p.lineTo (0.3f, 0.38f);
                p.quadraticTo (0.3f, 0.62f, 0.5f, 0.62f);
                p.quadraticTo (0.7f, 0.62f, 0.7f, 0.38f);
                p.lineTo (0.7f, 0.05f);
                p.startNewSubPath (0.5f, 0.62f); p.lineTo (0.5f, 0.84f);
                f.addEllipse (0.44f, 0.84f, 0.12f, 0.12f);
                break;
            case Icon::save:
                p.startNewSubPath (0.1f, 0.1f); p.lineTo (0.72f, 0.1f); p.lineTo (0.9f, 0.28f);
                p.lineTo (0.9f, 0.9f); p.lineTo (0.1f, 0.9f); p.closeSubPath();
                p.addRectangle (0.3f, 0.55f, 0.4f, 0.35f);
                p.addRectangle (0.3f, 0.1f, 0.34f, 0.22f);
                break;
            case Icon::trash:
                p.startNewSubPath (0.25f, 0.3f); p.lineTo (0.3f, 0.92f); p.lineTo (0.7f, 0.92f); p.lineTo (0.75f, 0.3f);
                p.startNewSubPath (0.15f, 0.26f); p.lineTo (0.85f, 0.26f);
                p.startNewSubPath (0.4f, 0.26f); p.lineTo (0.4f, 0.12f); p.lineTo (0.6f, 0.12f); p.lineTo (0.6f, 0.26f);
                p.startNewSubPath (0.42f, 0.42f); p.lineTo (0.42f, 0.8f);
                p.startNewSubPath (0.58f, 0.42f); p.lineTo (0.58f, 0.8f);
                break;
            case Icon::plus:
                p.startNewSubPath (0.5f, 0.12f); p.lineTo (0.5f, 0.88f);
                p.startNewSubPath (0.12f, 0.5f); p.lineTo (0.88f, 0.5f);
                break;
            case Icon::gear:
            {
                for (int i = 0; i < 8; ++i)   // eight teeth around a ring
                {
                    const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
                    p.startNewSubPath (0.5f + 0.30f * std::sin (a), 0.5f - 0.30f * std::cos (a));
                    p.lineTo (0.5f + 0.44f * std::sin (a), 0.5f - 0.44f * std::cos (a));
                }
                p.addEllipse (0.2f, 0.2f, 0.6f, 0.6f);
                p.addEllipse (0.38f, 0.38f, 0.24f, 0.24f);
                break;
            }
            case Icon::prev:
                p.startNewSubPath (0.65f, 0.1f); p.lineTo (0.3f, 0.5f); p.lineTo (0.65f, 0.9f);
                break;
            case Icon::next:
                p.startNewSubPath (0.35f, 0.1f); p.lineTo (0.7f, 0.5f); p.lineTo (0.35f, 0.9f);
                break;
        }
    }

    Icon icon;
    juce::String caption;
};

// Vertical pill switch with its two positions written beside it (top = on, bottom = off).
class PillSwitch : public juce::Button
{
public:
    PillSwitch (const juce::String& onLabel, const juce::String& offLabel) : juce::Button (onLabel), onText (onLabel), offText (offLabel) {}

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        const auto r = getLocalBounds().toFloat();
        const auto pill = juce::Rectangle<float> (26.0f, r.getHeight() - 4.0f).withPosition (r.getX() + 2.0f, r.getY() + 2.0f);
        const bool on = getToggleState();

        g.setColour (Palette::feltDeep);
        g.fillRoundedRectangle (pill, 13.0f);
        g.setColour (Palette::brass.withAlpha (highlighted ? 1.0f : 0.8f));
        g.drawRoundedRectangle (pill, 13.0f, 2.0f);
        const float d = 15.0f;
        const auto ball = juce::Rectangle<float> (d, d).withCentre ({ pill.getCentreX(), on ? pill.getY() + 6.0f + d * 0.5f : pill.getBottom() - 6.0f - d * 0.5f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff4d6), ball.getX() + 3.0f, ball.getY() + 3.0f,
                                                 juce::Colour (0xffb7a377), ball.getRight(), ball.getBottom(), true));
        g.fillEllipse (ball);

        const auto words = r.withTrimmedLeft (40.0f);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.setColour (on ? Palette::amber : Palette::dim);
        g.drawText (onText, words.withHeight (r.getHeight() * 0.5f).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (on ? Palette::dim : Palette::amber);
        g.drawText (offText, words.withTop (r.getCentreY()).toNearestInt(), juce::Justification::centredLeft);
    }

private:
    juce::String onText, offText;
};

// Pedal footswitch: a hex nut under a brushed aluminum cap, with a small LED beside it that lights in the pedal's color.
class StompSwitch : public juce::Button
{
public:
    explicit StompSwitch (juce::Colour led) : juce::Button ("stomp"), ledColour (led) { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto r = getLocalBounds().toFloat();
        const bool on = getToggleState();
        const float d = juce::jmin (r.getHeight() - 8.0f, 66.0f);
        const auto c = r.getCentre();

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - d * 0.5f + 2.0f, c.y - d * 0.5f + 4.0f, d, d);

        juce::Path nut;
        nut.addPolygon (c, 6, d * 0.52f, juce::MathConstants<float>::pi / 6.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc4c7cc), c.x - d * 0.4f, c.y - d * 0.5f,
                                                 juce::Colour (0xff4a4d53), c.x + d * 0.5f, c.y + d * 0.5f, false));
        g.fillPath (nut);
        g.setColour (juce::Colour (0xff25272b));
        g.strokePath (nut, juce::PathStrokeType (1.2f));

        const float cap = d * 0.78f;
        const float lift = down ? 0.0f : 2.5f;
        g.setColour (juce::Colour (0xff6a6e75));
        g.fillEllipse (c.x - cap * 0.5f, c.y - cap * 0.5f + 3.0f - lift * 0.4f, cap, cap);
        const auto top = juce::Rectangle<float> (cap, cap).withCentre ({ c.x, c.y - lift });
        g.setGradientFill (juce::ColourGradient (highlighted ? juce::Colours::white : juce::Colour (0xfff2f3f5), top.getX() + cap * 0.2f, top.getY(),
                                                 juce::Colour (0xff9da1a8), top.getRight(), top.getBottom(), false));
        g.fillEllipse (top);
        g.setColour (juce::Colours::black.withAlpha (0.07f));
        for (float f = 0.2f; f < 0.97f; f += 0.08f)
            g.drawEllipse (top.withSizeKeepingCentre (cap * f, cap * f), 0.8f);
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawEllipse (top.reduced (1.0f), 1.3f);
        g.setColour (juce::Colour (0xff4b4e54));
        g.drawEllipse (top, 1.0f);

        // LED to the right of the switch
        const float ledD = 11.0f;
        const auto led = juce::Rectangle<float> (ledD, ledD).withCentre ({ c.x + d * 0.5f + 20.0f, c.y });
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (ledColour.withAlpha (0.6f), led.getCentreX(), led.getCentreY(),
                                                     ledColour.withAlpha (0.0f), led.getCentreX() + ledD * 2.0f, led.getCentreY(), true));
            g.fillEllipse (led.expanded (ledD * 1.8f));
        }
        g.setColour (juce::Colour (0xff1b1c1f));
        g.fillEllipse (led.expanded (2.5f));
        g.setColour (on ? ledColour.brighter (0.3f) : ledColour.darker (0.9f));
        g.fillEllipse (led);
        if (on)
        {
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.fillEllipse (led.getX() + 2.5f, led.getY() + 2.0f, 3.5f, 2.5f);
        }
    }

private:
    juce::Colour ledColour;
};

// Thin vertical level meter. The audio thread stores block peaks in `source`; this reads and clears them.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (std::atomic<float>& peakSource) : source (peakSource) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        // a column of small lamps in a brass frame: cream, then amber near the top, red at clipping
        const auto r = getLocalBounds().toFloat();
        g.setColour (Palette::feltDeep);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Palette::brass.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        const auto inner = r.reduced (2.5f, 3.0f);
        const int count = juce::jmax (6, (int) (inner.getHeight() / 7.0f));
        const float segH = inner.getHeight() / (float) count;
        for (int k = 0; k < count; ++k)
        {
            const float segDb = -60.0f + 60.0f * (float) (k + 1) / (float) count;
            const auto colour = segDb > -1.5f ? Palette::red : segDb > -8.0f ? Palette::amber : Palette::text;
            const bool lit = norm * (float) count > (float) k;
            const auto seg = juce::Rectangle<float> (inner.getX(), inner.getBottom() - segH * (float) (k + 1), inner.getWidth(), segH).reduced (0.0f, 1.0f);
            g.setColour (lit ? colour : colour.withAlpha (0.10f));
            g.fillRoundedRectangle (seg, 1.5f);
            if (lit)
            {
                g.setColour (colour.withAlpha (0.25f));
                g.fillRoundedRectangle (seg.expanded (1.5f, 1.0f), 2.0f);
            }
        }
    }

private:
    void timerCallback() override
    {
        const float peak = source.exchange (0.0f);
        const float next = juce::jmax (peak, level * 0.85f);
        if (std::abs (next - level) > 1.0e-4f)
        {
            level = next;
            repaint();
        }
    }

    std::atomic<float>& source;
    float level = 0.0f;
};

// Three-way selector like a guitar's pickup switch: a chrome lever in a black plate, pointing left, up or right.
// Position 0 is the left label, 1 the middle, 2 the right. Click a side, drag the lever or turn the mouse wheel.
class SelectorSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    SelectorSwitch (juce::StringArray positionNames) : names (std::move (positionNames)) {}

    std::function<void (int)> onChange;
    int getPosition() const { return position; }
    void setPosition (int p) { position = juce::jlimit (0, 2, p); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const auto label = r.removeFromBottom (15.0f);
        const float rad = juce::jmin (r.getWidth() * 0.5f - 2.0f, r.getHeight() - 8.0f);
        const juce::Point<float> pivot (r.getCentreX(), r.getBottom() - 4.0f);

        // black plate with a cream rim, three amber lamps for the positions
        juce::Path half;
        half.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f,
                            -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        half.closeSubPath();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillPath (half, juce::AffineTransform::translation (0.0f, 2.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2a2c), pivot.x, pivot.y - rad, juce::Colour (0xff0b0b0c), pivot.x, pivot.y, false));
        g.fillPath (half);
        g.setColour (Palette::cream.withAlpha (0.75f));
        g.strokePath (half, juce::PathStrokeType (1.2f));

        const float reach = rad - 8.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float a = angleFor (i);
            const auto dot = pivot + juce::Point<float> (std::sin (a), -std::cos (a)) * (reach + 1.0f);
            const bool lit = i == position;
            if (lit)
            {
                g.setGradientFill (juce::ColourGradient (Palette::amber.withAlpha (0.6f), dot.x, dot.y, Palette::amber.withAlpha (0.0f), dot.x + 8.0f, dot.y, true));
                g.fillEllipse (dot.x - 8.0f, dot.y - 8.0f, 16.0f, 16.0f);
            }
            g.setColour (lit ? Palette::amber : Palette::amber.withAlpha (0.25f));
            g.fillEllipse (dot.x - 2.6f, dot.y - 2.6f, 5.2f, 5.2f);
        }

        // the lever: chrome shaft with a cream tip, casting a shadow
        const float a = angleFor (position);
        const auto dir = juce::Point<float> (std::sin (a), -std::cos (a));
        const auto tip = pivot + dir * (reach - 7.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawLine ({ pivot.translated (2.0f, 2.0f), tip.translated (2.0f, 2.0f) }, 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f3f5), pivot.x - 2.0f, pivot.y, juce::Colour (0xff7d8189), pivot.x + 3.0f, pivot.y, false));
        g.drawLine ({ pivot, tip }, 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff6dc), tip.x - 3.0f, tip.y - 3.0f, juce::Colour (0xffb9a676), tip.x + 4.0f, tip.y + 4.0f, true));
        g.fillEllipse (tip.x - 6.0f, tip.y - 6.0f, 12.0f, 12.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe5e6e8), pivot.x - 3.0f, pivot.y - 3.0f, juce::Colour (0xff55585e), pivot.x + 4.0f, pivot.y + 4.0f, true));
        g.fillEllipse (pivot.x - 5.0f, pivot.y - 5.0f, 10.0f, 10.0f);   // pivot nut

        g.setColour (Palette::cream);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (names[position], label.toNearestInt(), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override { choose (positionAt (e.position.x)); }
    void mouseDrag (const juce::MouseEvent& e) override { choose (positionAt (e.position.x)); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const float d = (w.isReversed ? -1.0f : 1.0f) * w.deltaY;
        if (d != 0.0f)
            choose (position + (d > 0.0f ? 1 : -1));
    }

private:
    static float angleFor (int p) { return juce::degreesToRadians (-40.0f + 40.0f * (float) p); }
    int positionAt (float x) const { return juce::jlimit (0, 2, (int) (3.0f * x / (float) juce::jmax (1, getWidth()))); }

    void choose (int p)
    {
        p = juce::jlimit (0, 2, p);
        if (p == position)
            return;
        setPosition (p);
        if (onChange)
            onChange (p);
    }

    juce::StringArray names;
    int position = 0;
};
