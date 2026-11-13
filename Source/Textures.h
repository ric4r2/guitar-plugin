#pragma once
#include <juce_graphics/juce_graphics.h>

// Procedural surface textures, generated once per size.
namespace Textures
{
    // The frame: dark green with a fine grain, a soft light in the middle and darker corners.
    inline juce::Image felt (int w, int h)
    {
        juce::Image img (juce::Image::RGB, w, h, true);
        juce::Graphics g (img);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b3a2a), (float) w * 0.5f, (float) h * 0.38f,
                                                 juce::Colour (0xff07140d), 0.0f, 0.0f, true));
        g.fillAll();

        juce::Random rng (1974);
        for (int i = 0; i < w * h / 6; ++i)
        {
            const int x = rng.nextInt (w), y = rng.nextInt (h);
            g.setColour (rng.nextBool() ? juce::Colour (0x0affffff) : juce::Colour (0x12000000));
            g.fillRect (x, y, 1, 1);
        }
        g.setColour (juce::Colour (0x06f1e6c4));   // faint horizontal weave
        for (int y = 0; y < h; y += 3)
            g.drawHorizontalLine (y, 0.0f, (float) w);
        return img;
    }

    // Tweed amp covering: cream-yellow weave with brown stripes down both sides.
    inline juce::Image tweed (int w, int h)
    {
        juce::Image img (juce::Image::RGB, w, h, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xffd9c78f));

        juce::Random rng (7);
        const juce::Colour threads[] = { juce::Colour (0xffb09250).withAlpha (0.55f),
                                         juce::Colour (0xff7d5b2c).withAlpha (0.35f),
                                         juce::Colour (0xffefe0aa).withAlpha (0.5f) };
        for (int y = 0; y < h; y += 3)
            for (int x = rng.nextInt (6); x < w; x += 6 + rng.nextInt (6))
            {
                g.setColour (threads[rng.nextInt (3)]);
                g.fillRect (x, y, 3 + rng.nextInt (3), 1);
            }

        g.setColour (juce::Colour (0xffb09250).withAlpha (0.12f));
        for (int x = 0; x < w; x += 4)
            g.drawVerticalLine (x, 0.0f, (float) h);

        const auto stripe = [&] (int x)
        {
            g.setColour (juce::Colour (0xff5e3417));
            g.fillRect (x, 0, 5, h);
            g.setColour (juce::Colour (0xffefe2b8).withAlpha (0.6f));
            g.fillRect (x + 7, 0, 2, h);
        };
        stripe (3);
        stripe (w - 11);
        return img;
    }

    // Brushed aluminum: a vertical sheen with long, faint horizontal scratches.
    inline juce::Image brushedAluminum (int w, int h, int seed)
    {
        juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
        juce::Graphics g (img);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd7dade), 0.0f, 0.0f,
                                                 juce::Colour (0xff9da1a8), 0.0f, (float) h, false));
        g.fillAll();

        juce::Random rng (seed);
        for (int i = 0; i < h * 7; ++i)
        {
            const int y = rng.nextInt (h), x = rng.nextInt (w), len = 30 + rng.nextInt (280);
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colour (0xff3f4247)).withAlpha (0.04f + rng.nextFloat() * 0.11f));
            g.drawHorizontalLine (y, (float) x, (float) (x + len));
        }
        return img;
    }

    // Speaker grille cloth: wheat-coloured weave.
    inline juce::Image grille (int w, int h)
    {
        juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xffc4a66a));

        g.setColour (juce::Colour (0xff5b4526).withAlpha (0.35f));
        for (int x = 0; x < w; x += 3)
            g.drawVerticalLine (x, 0.0f, (float) h);
        g.setColour (juce::Colour (0xff5b4526).withAlpha (0.3f));
        for (int y = 0; y < h; y += 3)
            g.drawHorizontalLine (y, 0.0f, (float) w);

        juce::Random rng (11);
        for (int i = 0; i < w * h / 40; ++i)
        {
            g.setColour ((rng.nextBool() ? juce::Colour (0xff3b2a14) : juce::Colour (0xffe6cf94)).withAlpha (0.35f));
            g.fillRect (rng.nextInt (w), rng.nextInt (h), 2, 2);
        }

        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.28f), 0.0f, 0.0f,
                                                 juce::Colours::transparentBlack, 0.0f, (float) h * 0.5f, false));
        g.fillAll();
        return img;
    }

    // Pedalboard carpet.
    inline juce::Image carpet (int w, int h)
    {
        juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff1f2024));

        juce::Random rng (21);
        for (int i = 0; i < w * h / 5; ++i)
        {
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.03f + rng.nextFloat() * 0.07f));
            g.fillRect (rng.nextInt (w), rng.nextInt (h), 1 + rng.nextInt (2), 1);
        }
        return img;
    }

    // Window background outside the amp: dark graphite with a faint grain.
    inline juce::Image graphite (int w, int h)
    {
        juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
        juce::Graphics g (img);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff23252a), 0.0f, 0.0f, juce::Colour (0xff121316), 0.0f, (float) h, false));
        g.fillAll();

        juce::Random rng (3);
        for (int i = 0; i < w * h / 30; ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (rng.nextFloat() * 0.035f));
            g.fillRect (rng.nextInt (w), rng.nextInt (h), 1, 1);
        }
        return img;
    }
}
