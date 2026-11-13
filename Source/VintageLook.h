#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace Palette
{
    // neutral frame around everything except the amp
    const juce::Colour graphite      { 0xff17181b };
    const juce::Colour graphiteMid   { 0xff24262a };
    const juce::Colour graphiteLight { 0xff32353b };
    const juce::Colour silver        { 0xffb7bac1 };
    const juce::Colour text          { 0xffe6e7ea };
    const juce::Colour dim           { 0xff8c8f96 };
    const juce::Colour inkDark       { 0xff1d1f23 };

    // vintage amp
    const juce::Colour plate         { 0xff241911 };
    const juce::Colour plateLight    { 0xff3a2a1d };
    const juce::Colour gold          { 0xffc9a24a };
    const juce::Colour cream         { 0xffefe2b8 };

    // shared
    const juce::Colour amber         { 0xffe8a634 };
    const juce::Colour green         { 0xff86b84e };
    const juce::Colour red           { 0xffc4452f };
}

enum class KnobStyle { chrome, strat };

// Rotary knob driven by the mouse wheel: each notch moves a fraction of the range, Shift uses a finer one.
// It draws itself as a chrome knob with a spike pointer, or as a cream Stratocaster barrel knob.
class ScrollKnob : public juce::Slider
{
public:
    ScrollKnob()
        : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow)
    {
        setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    }

    void setLook (KnobStyle s, juce::Colour body, juce::Colour scale)
    {
        style = s;
        bodyColour = body;
        scaleColour = scale;
        repaint();
    }

    KnobStyle getStyle() const { return style; }
    juce::Colour getBodyColour() const { return bodyColour; }
    juce::Colour getScaleColour() const { return scaleColour; }

    void setWheelFractions (double normal, double fine) { wheelStep = normal; wheelFine = fine; }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const float delta = (wheel.isReversed ? -1.0f : 1.0f) * wheel.deltaY;
        if (delta == 0.0f)
            return;

        const double step = e.mods.isShiftDown() ? wheelFine : wheelStep;
        const double prop = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + (delta > 0 ? step : -step));
        setValue (proportionOfLengthToValue (prop), juce::sendNotificationSync);
    }

private:
    KnobStyle style = KnobStyle::chrome;
    juce::Colour bodyColour { 0xffe9eaec }, scaleColour { Palette::text };
    double wheelStep = 0.03, wheelFine = 0.005;
};

class VintageLook : public juce::LookAndFeel_V4
{
public:
    VintageLook()
    {
        setColour (juce::Slider::textBoxTextColourId, Palette::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, Palette::text);
        setColour (juce::TextButton::buttonColourId, Palette::graphiteLight);
        setColour (juce::TextButton::buttonOnColourId, Palette::amber);
        setColour (juce::TextButton::textColourOffId, Palette::text);
        setColour (juce::TextButton::textColourOnId, Palette::inkDark);
        setColour (juce::ComboBox::backgroundColourId, Palette::graphiteLight);
        setColour (juce::ComboBox::textColourId, Palette::text);
        setColour (juce::ComboBox::outlineColourId, Palette::silver);
        setColour (juce::ComboBox::arrowColourId, Palette::silver);
        setColour (juce::PopupMenu::backgroundColourId, Palette::graphiteMid);
        setColour (juce::PopupMenu::textColourId, Palette::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::amber);
        setColour (juce::PopupMenu::highlightedTextColourId, Palette::inkDark);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto* knob = dynamic_cast<ScrollKnob*> (&slider);
        const auto style = knob != nullptr ? knob->getStyle() : KnobStyle::chrome;
        const auto scale = knob != nullptr ? knob->getScaleColour() : Palette::text;

        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // printed scale
        g.setColour (scale.withAlpha (0.8f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            const float s = std::sin (a), c = -std::cos (a);
            const bool major = i % 5 == 0;
            g.drawLine (centre.x + s * radius * 0.84f, centre.y + c * radius * 0.84f,
                        centre.x + s * radius * (major ? 1.0f : 0.94f), centre.y + c * radius * (major ? 1.0f : 0.94f),
                        major ? 2.0f : 1.2f);
        }

        if (style == KnobStyle::chrome)
            drawChromeKnob (g, centre, radius * 0.72f, angle);
        else
            drawStratKnob (g, centre, radius * 0.70f, angle, knob != nullptr ? knob->getBodyColour() : Palette::cream);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
            return;
        }

        const float cx = (float) x + (float) width * 0.5f;
        const auto tick = slider.findColour (juce::Slider::backgroundColourId);
        const auto cap = slider.findColour (juce::Slider::thumbColourId);

        g.setColour (juce::Colour (0xff0e0f11));
        g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) height, 3.0f);

        for (int i = 0; i <= 4; ++i)
        {
            const float ty = (float) y + (float) height * (float) i / 4.0f;
            const float half = i == 2 ? 19.0f : 13.0f;
            g.setColour (tick.withAlpha (i == 2 ? 0.95f : 0.6f));
            g.drawLine (cx - half, ty, cx + half, ty, i == 2 ? 1.6f : 1.0f);
        }

        const auto thumb = juce::Rectangle<float> (34.0f, 17.0f).withCentre ({ cx, sliderPos });
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 2.0f), 3.0f);
        g.setGradientFill (juce::ColourGradient (cap.brighter (0.3f), thumb.getX(), thumb.getY(),
                                                 cap.darker (0.2f), thumb.getX(), thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (cap.darker (0.7f));
        g.drawLine (thumb.getX() + 4.0f, sliderPos, thumb.getRight() - 4.0f, sliderPos, 2.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto base = b.findColour (b.getToggleState() ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId);
        if (down)
            base = base.darker (0.2f);
        else if (highlighted)
            base = base.brighter (0.12f);

        g.setGradientFill (juce::ColourGradient (base.brighter (0.15f), r.getX(), r.getY(), base.darker (0.2f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (b.findColour (juce::ComboBox::outlineColourId).withAlpha (b.getToggleState() ? 1.0f : 0.7f));
        g.drawRoundedRectangle (r, 5.0f, 1.2f);
    }

private:
    // Chrome knob: knurled skirt, bright domed cap and a black spike that shows where it points.
    static void drawChromeKnob (juce::Graphics& g, juce::Point<float> c, float r, float angle)
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - r + 1.5f, c.y - r + 3.0f, r * 2.0f, r * 2.0f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6f7f9), c.x - r * 0.5f, c.y - r * 0.6f,
                                                 juce::Colour (0xff5d6167), c.x + r * 0.8f, c.y + r * 0.9f, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        g.setColour (juce::Colour (0xff383b40).withAlpha (0.6f));
        for (int i = 0; i < 48; ++i)
        {
            const float a = (float) i * twoPi / 48.0f;
            g.drawLine (c.x + std::sin (a) * r * 0.90f, c.y - std::cos (a) * r * 0.90f,
                        c.x + std::sin (a) * r * 0.99f, c.y - std::cos (a) * r * 0.99f, 1.0f);
        }
        g.setColour (juce::Colour (0xff2b2d31));
        g.drawEllipse (c.x - r * 0.86f, c.y - r * 0.86f, r * 1.72f, r * 1.72f, 1.2f);

        const float cr = r * 0.80f;
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x - cr * 0.6f, c.y - cr * 0.7f,
                                                 juce::Colour (0xff9da1a8), c.x + cr * 0.7f, c.y + cr * 0.8f, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.06f));
        for (float f = 0.2f; f < 0.8f; f += 0.12f)
            g.drawEllipse (c.x - cr * f, c.y - cr * f, cr * f * 2.0f, cr * f * 2.0f, 1.0f);

        juce::Path crescent;
        crescent.addCentredArc (c.x, c.y, cr * 0.86f, cr * 0.86f, 0.0f, -1.9f, -0.4f, true);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.strokePath (crescent, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path spike;
        spike.startNewSubPath (0.0f, -cr * 0.97f);
        spike.lineTo (cr * 0.13f, -cr * 0.08f);
        spike.lineTo (-cr * 0.13f, -cr * 0.08f);
        spike.closeSubPath();
        const auto place = juce::AffineTransform::rotation (angle).translated (c);
        g.setColour (juce::Colour (0xff15161a));
        g.fillPath (spike, place);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.strokePath (spike, juce::PathStrokeType (0.8f), place);

        g.setColour (juce::Colour (0xff25272b));
        g.fillEllipse (c.x - cr * 0.12f, c.y - cr * 0.12f, cr * 0.24f, cr * 0.24f);
    }

    // Stratocaster barrel knob seen from above: creamy plastic with a glossy top and a dark pointer line.
    static void drawStratKnob (juce::Graphics& g, juce::Point<float> c, float r, float angle, juce::Colour body)
    {
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (c.x - r + 1.5f, c.y - r + 3.0f, r * 2.0f, r * 2.0f);

        g.setGradientFill (juce::ColourGradient (body.brighter (0.1f), c.x - r * 0.4f, c.y - r * 0.5f,
                                                 body.darker (0.35f), c.x + r * 0.7f, c.y + r * 0.8f, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (body.darker (0.55f));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.3f);

        const float tr = r * 0.80f;
        g.setGradientFill (juce::ColourGradient (body.brighter (0.4f), c.x - tr * 0.5f, c.y - tr * 0.6f,
                                                 body.darker (0.05f), c.x + tr * 0.6f, c.y + tr * 0.7f, true));
        g.fillEllipse (c.x - tr, c.y - tr, tr * 2.0f, tr * 2.0f);
        g.setColour (body.darker (0.3f).withAlpha (0.55f));
        g.drawEllipse (c.x - tr, c.y - tr, tr * 2.0f, tr * 2.0f, 1.0f);
        g.drawEllipse (c.x - tr * 0.55f, c.y - tr * 0.55f, tr * 1.1f, tr * 1.1f, 0.8f);

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.55f), c.x - tr * 0.45f, c.y - tr * 0.55f,
                                                 juce::Colours::transparentWhite, c.x - tr * 0.05f, c.y - tr * 0.1f, true));
        g.fillEllipse (c.x - tr * 0.8f, c.y - tr * 0.85f, tr * 0.9f, tr * 0.7f);

        juce::Path pointer;
        pointer.startNewSubPath (0.0f, -tr * 0.25f);
        pointer.lineTo (0.0f, -tr * 0.9f);
        g.setColour (body.darker (0.85f));
        g.strokePath (pointer, juce::PathStrokeType (juce::jmax (2.5f, r * 0.11f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::rotation (angle).translated (c));
    }
};

// Vertical fader for the equalizer: the wheel moves it half a dB per notch (Shift = a tenth).
class ScrollFader : public juce::Slider
{
public:
    ScrollFader()
        : juce::Slider (juce::Slider::LinearVertical, juce::Slider::TextBoxBelow)
    {
        setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        const float delta = (wheel.isReversed ? -1.0f : 1.0f) * wheel.deltaY;
        if (delta == 0.0f)
            return;

        const double step = e.mods.isShiftDown() ? 0.1 : 0.5;
        setValue (juce::jlimit (getMinimum(), getMaximum(), getValue() + (delta > 0 ? step : -step)), juce::sendNotificationSync);
    }
};

// Lamp switch with a label, used for the metronome.
class LedToggle : public juce::Button
{
public:
    explicit LedToggle (const juce::String& text) : juce::Button (text) { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool highlighted, bool) override
    {
        const auto r = getLocalBounds().toFloat();
        const float d = juce::jmin (r.getHeight() * 0.5f, 16.0f);
        const auto led = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + d, r.getCentreY() });
        const bool on = getToggleState();

        if (on)
        {
            g.setColour (Palette::amber.withAlpha (0.3f));
            g.fillEllipse (led.expanded (4.0f));
        }
        g.setColour (on ? Palette::amber : Palette::graphiteLight.brighter (0.1f));
        g.fillEllipse (led);
        g.setColour (Palette::silver);
        g.drawEllipse (led, 1.2f);

        g.setColour (highlighted ? Palette::text : Palette::text.withAlpha (0.85f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (getButtonText(), r.withTrimmedLeft (d * 2.0f + 4.0f).toNearestInt(), juce::Justification::centredLeft);
    }
};

// Stompbox footswitch: a colored LED that lights when the pedal is engaged, and a chrome dome to press.
class FootSwitch : public juce::Button
{
public:
    explicit FootSwitch (juce::Colour led) : juce::Button ("footswitch"), ledColour (led) { setClickingTogglesState (true); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto r = getLocalBounds().toFloat();
        const bool on = getToggleState();

        // LED
        const float ledD = 15.0f;
        const auto led = juce::Rectangle<float> (ledD, ledD).withCentre ({ r.getCentreX(), r.getY() + 12.0f });
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (ledColour.withAlpha (0.55f), led.getCentreX(), led.getCentreY(),
                                                     ledColour.withAlpha (0.0f), led.getCentreX() + ledD * 2.0f, led.getCentreY(), true));
            g.fillEllipse (led.expanded (ledD * 2.0f));
        }
        g.setColour (juce::Colour (0xff1b1c1f));
        g.fillEllipse (led.expanded (3.0f));
        g.setGradientFill (juce::ColourGradient (on ? ledColour.brighter (0.5f) : ledColour.darker (0.85f), led.getCentreX() - 2.0f, led.getCentreY() - 3.0f,
                                                 on ? ledColour : ledColour.darker (0.95f), led.getCentreX() + 3.0f, led.getCentreY() + 4.0f, true));
        g.fillEllipse (led);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.15f));
        g.fillEllipse (led.getX() + 3.0f, led.getY() + 2.5f, 4.5f, 3.0f);

        // chrome dome
        const float room = r.getHeight() - (led.getBottom() + 12.0f) - 6.0f;
        const float d = juce::jmin (r.getWidth() * 0.86f, room) * (down ? 0.96f : 1.0f);
        const juce::Point<float> c (r.getCentreX(), led.getBottom() + 12.0f + room * 0.5f);

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - d * 0.5f + 1.5f, c.y - d * 0.5f + 3.0f, d, d);
        g.setColour (juce::Colour (0xff2c2e33));
        g.fillEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
        const float cap = d * 0.86f;
        g.setGradientFill (juce::ColourGradient (highlighted ? juce::Colours::white : juce::Colour (0xfff2f3f5), c.x - cap * 0.4f, c.y - cap * 0.5f,
                                                 juce::Colour (0xff80848b), c.x + cap * 0.5f, c.y + cap * 0.6f, true));
        g.fillEllipse (c.x - cap * 0.5f, c.y - cap * 0.5f, cap, cap);
        g.setColour (juce::Colours::black.withAlpha (0.08f));
        for (float f = 0.25f; f < 0.95f; f += 0.15f)
            g.drawEllipse (c.x - cap * f * 0.5f, c.y - cap * f * 0.5f, cap * f, cap * f, 1.0f);
    }

private:
    juce::Colour ledColour;
};

// How a group of knobs is dressed.
struct KnobTheme
{
    KnobStyle style;
    juce::Colour body, label, value, scale;
};

namespace Themes
{
    inline KnobTheme amp()                 { return { KnobStyle::chrome, juce::Colours::white, Palette::cream, Palette::cream, Palette::cream }; }
    inline KnobTheme neutral()             { return { KnobStyle::chrome, juce::Colours::white, Palette::dim, Palette::text, Palette::silver }; }
    inline KnobTheme pedal (juce::Colour c) { return { KnobStyle::strat, c, Palette::inkDark, Palette::inkDark, Palette::inkDark }; }
}
