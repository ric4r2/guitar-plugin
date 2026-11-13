#pragma once
#include <juce_graphics/juce_graphics.h>

// The artist logo: "Joe" and "Garcia" on two staggered lines, italic script with a chrome finish.
namespace Logo
{
    inline const char* defaultFont() { return "Brush Script MT"; }

    // Draws on a transparent background. `fontName` falls back to a bold italic sans if it isn't installed.
    inline juce::Image render (int width, int height, const juce::String& fontName = defaultFont())
    {
        juce::Image img (juce::Image::ARGB, width, height, true);
        juce::Graphics g (img);

        const auto installed = juce::Font::findAllTypefaceNames();
        const auto makeFont = [&] (float h)
        {
            if (installed.contains (fontName))
                return juce::Font (juce::FontOptions (fontName, h, juce::Font::bold | juce::Font::italic));
            return juce::Font (juce::FontOptions (h, juce::Font::bold | juce::Font::italic));
        };

        const auto outline = [&] (const juce::String& text)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (makeFont (100.0f), text, 0.0f, 0.0f);
            juce::Path p;
            ga.createPath (p);
            return p;
        };

        auto first = outline ("Joe");
        auto second = outline ("Garcia");
        const auto b1 = first.getBounds(), b2 = second.getBounds();

        // fit both lines into the image, the second one pushed to the right
        const float stagger = 0.16f, gap = 0.06f;
        const float totalW = juce::jmax (b1.getWidth(), b2.getWidth() + stagger * juce::jmax (b1.getWidth(), b2.getWidth()));
        const float totalH = b1.getHeight() + b2.getHeight() + gap * b1.getHeight();
        const float margin = (float) juce::jmin (width, height) * 0.06f;
        const float scale = juce::jmin (((float) width - margin * 2.0f) / totalW, ((float) height - margin * 2.0f) / totalH);

        const float contentW = totalW * scale, contentH = totalH * scale;
        const float x0 = ((float) width - contentW) * 0.5f, y0 = ((float) height - contentH) * 0.5f;

        first.applyTransform (juce::AffineTransform::translation (-b1.getX(), -b1.getY())
                                  .scaled (scale).translated (x0, y0));
        second.applyTransform (juce::AffineTransform::translation (-b2.getX(), -b2.getY())
                                   .scaled (scale)
                                   .translated (x0 + stagger * juce::jmax (b1.getWidth(), b2.getWidth()) * scale,
                                                y0 + (b1.getHeight() + gap * b1.getHeight()) * scale));

        for (auto* path : { &first, &second })
        {
            const auto bounds = path->getBounds();
            const float lineH = bounds.getHeight();

            // soft shadow underneath
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (juce::Colours::black.withAlpha (0.16f));
                g.strokePath (*path, juce::PathStrokeType (lineH * 0.05f * (float) i, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                              juce::AffineTransform::translation (lineH * 0.025f, lineH * 0.045f));
            }

            // dark edge
            g.setColour (juce::Colour (0xff12161b));
            g.strokePath (*path, juce::PathStrokeType (lineH * 0.065f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // chrome: sky on top, a dark horizon, warm ground reflection below
            juce::ColourGradient chrome (juce::Colour (0xff7f9db9), 0.0f, bounds.getY(), juce::Colour (0xffa8a091), 0.0f, bounds.getBottom(), false);
            chrome.addColour (0.30, juce::Colour (0xffe9f1f8));
            chrome.addColour (0.46, juce::Colours::white);
            chrome.addColour (0.51, juce::Colour (0xff2a3441));
            chrome.addColour (0.58, juce::Colour (0xff6b7a8a));
            chrome.addColour (0.80, juce::Colour (0xffd8cfc0));
            g.setGradientFill (chrome);
            g.fillPath (*path);

            // bright inner rim
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.strokePath (*path, juce::PathStrokeType (juce::jmax (1.0f, lineH * 0.012f)), juce::AffineTransform::translation (-lineH * 0.006f, -lineH * 0.008f));
        }
        return img;
    }
}
