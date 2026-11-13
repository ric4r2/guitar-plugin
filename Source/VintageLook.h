#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Tweed-amp palette: cream-and-brown weave on the outside, dark leatherette control panels, brass trim.
namespace Palette
{
    const juce::Colour tweed      { 0xffd9c78f };
    const juce::Colour tweedDark  { 0xffb09250 };
    const juce::Colour stripe     { 0xff5e3417 };
    const juce::Colour panel      { 0xff2a1c13 };
    const juce::Colour panelLight { 0xff3b2a1d };
    const juce::Colour gold       { 0xffc9a24a };
    const juce::Colour cream      { 0xffefe2b8 };
    const juce::Colour muted      { 0xffa8946b };
    const juce::Colour amber      { 0xffe8a634 };
    const juce::Colour green      { 0xff86b84e };
    const juce::Colour red        { 0xffc4452f };
    const juce::Colour ink        { 0xff2b1b10 };
}

class VintageLook : public juce::LookAndFeel_V4
{
public:
    VintageLook()
    {
        setColour (juce::Slider::textBoxTextColourId, Palette::cream);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, Palette::cream);
        setColour (juce::TextButton::buttonColourId, Palette::panelLight);
        setColour (juce::TextButton::buttonOnColourId, Palette::amber);
        setColour (juce::TextButton::textColourOffId, Palette::cream);
        setColour (juce::TextButton::textColourOnId, Palette::ink);
        setColour (juce::ComboBox::backgroundColourId, Palette::panelLight);
        setColour (juce::ComboBox::textColourId, Palette::cream);
        setColour (juce::ComboBox::outlineColourId, Palette::gold);
        setColour (juce::ComboBox::arrowColourId, Palette::gold);
        setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
        setColour (juce::PopupMenu::textColourId, Palette::cream);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::amber);
        setColour (juce::PopupMenu::highlightedTextColourId, Palette::ink);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // scale ticks
        g.setColour (Palette::muted.withAlpha (0.75f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            const float s = std::sin (a), c = -std::cos (a);
            g.drawLine (centre.x + s * radius * 0.86f, centre.y + c * radius * 0.86f,
                        centre.x + s * radius, centre.y + c * radius, i % 5 == 0 ? 2.0f : 1.0f);
        }

        // value arc
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius * 0.78f, radius * 0.78f, 0.0f, startAngle, angle, true);
        g.setColour (Palette::amber);
        g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // black Bakelite knob with a brass edge
        const float kr = radius * 0.62f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4d433a), centre.x - kr * 0.4f, centre.y - kr * 0.5f,
                                                 juce::Colour (0xff0d0a08), centre.x + kr * 0.6f, centre.y + kr * 0.7f, true));
        g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (Palette::gold.withAlpha (0.85f));
        g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.8f, -kr * 0.92f, 3.6f, kr * 0.55f, 1.5f);
        g.setColour (Palette::cream);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
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
        g.setColour (Palette::ink);
        g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) height, 3.0f);

        for (int i = 0; i <= 4; ++i)
        {
            const float ty = (float) y + (float) height * (float) i / 4.0f;
            g.setColour (i == 2 ? Palette::cream.withAlpha (0.9f) : Palette::muted.withAlpha (0.6f));
            g.drawLine (cx - (i == 2 ? 18.0f : 13.0f), ty, cx + (i == 2 ? 18.0f : 13.0f), ty, i == 2 ? 1.6f : 1.0f);
        }

        const auto thumb = juce::Rectangle<float> (34.0f, 16.0f).withCentre ({ cx, sliderPos });
        g.setGradientFill (juce::ColourGradient (Palette::cream, thumb.getX(), thumb.getY(),
                                                 Palette::muted, thumb.getX(), thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (Palette::ink);
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
        g.setColour (Palette::gold.withAlpha (b.getToggleState() ? 1.0f : 0.7f));
        g.drawRoundedRectangle (r, 5.0f, 1.2f);
    }
};

// Rotary knob driven by the mouse wheel: each notch moves a fraction of the range, Shift uses a finer one.
class ScrollKnob : public juce::Slider
{
public:
    ScrollKnob()
        : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow)
    {
        setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    }

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
    double wheelStep = 0.03, wheelFine = 0.005;
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

// Pedal switch: a lit lamp and a label.
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
        g.setColour (on ? Palette::amber : Palette::panelLight.brighter (0.1f));
        g.fillEllipse (led);
        g.setColour (Palette::gold);
        g.drawEllipse (led, 1.2f);

        g.setColour (highlighted ? Palette::cream : Palette::cream.withAlpha (0.85f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (getButtonText(), r.withTrimmedLeft (d * 2.0f + 4.0f).toNearestInt(), juce::Justification::centredLeft);
    }
};
