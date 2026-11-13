#include "PluginEditor.h"

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
    const auto noteColour = ! haveNote ? Palette::muted : inTune ? Palette::green : Palette::amber;

    const juce::String noteText = haveNote ? juce::String (names[((note % 12) + 12) % 12]) + juce::String (note / 12 - 1) : "--";
    g.setColour (noteColour);
    g.setFont (juce::FontOptions (96.0f, juce::Font::bold));
    g.drawText (noteText, b.withTrimmedTop (20.0f).withHeight (110.0f).toNearestInt(), juce::Justification::centred);

    g.setColour (Palette::muted);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText (haveNote ? juce::String (hz, 1) + " Hz" : "Play a string", juce::Rectangle<int> (0, 132, getWidth(), 22),
                juce::Justification::centred);

    // meter: -50..+50 cents
    const float halfWidth = std::min (300.0f, b.getWidth() * 0.5f - 30.0f);
    const float cx = b.getCentreX(), meterY = 180.0f;
    g.setColour (Palette::ink);
    g.fillRoundedRectangle (cx - halfWidth, meterY, halfWidth * 2.0f, 18.0f, 4.0f);

    g.setColour (Palette::green.withAlpha (0.35f));
    g.fillRect (cx - halfWidth * 0.1f, meterY, halfWidth * 0.2f, 18.0f);   // +/-5 cents

    g.setFont (juce::FontOptions (12.0f));
    for (int c = -50; c <= 50; c += 10)
    {
        const float x = cx + halfWidth * (float) c / 50.0f;
        const bool major = c == 0 || std::abs (c) == 50;
        g.setColour (Palette::muted);
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

    g.setColour (Palette::cream);
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

    g.setColour (Palette::muted);
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

    for (int s = 0; s < 2; ++s)
    {
        auto& strip = strips[(size_t) s];

        for (size_t k = 0; k < defs.size(); ++k)
            strip.knobs[k].init (*this, apvts, GuitarDriveProcessor::paramId (s, defs[k].id), defs[k].label);
        strip.knobs[defs.size()].init (*this, apvts, GuitarDriveProcessor::paramId (s, GuitarDriveProcessor::panId), "PAN");

        // transpose moves in whole semitones: one wheel notch is one semitone
        strip.knobs[1].slider.setWheelFractions (1.0 / 24.0, 1.0 / 24.0);

        strip.name.setText (s == 0 ? "LEFT" : "RIGHT", juce::dontSendNotification);
        strip.name.setJustificationType (juce::Justification::centred);
        strip.name.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        strip.name.setColour (juce::Label::textColourId, Palette::cream);
        addAndMakeVisible (strip.name);

        strip.mute.setColour (juce::TextButton::buttonOnColourId, Palette::red);
        strip.mute.setColour (juce::TextButton::textColourOnId, Palette::cream);
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

void AmpView::paint (juce::Graphics& g)
{
    if (stereo)
    {
        g.setColour (Palette::gold.withAlpha (0.4f));
        g.drawHorizontalLine (getHeight() / 2, 16.0f, (float) getWidth() - 16.0f);
    }
}

void AmpView::layoutStrip (Strip& strip, juce::Rectangle<int> row, bool stereoRow)
{
    if (stereoRow)
        strip.name.setBounds (row.removeFromLeft (54));

    auto buttons = row.removeFromRight (104).reduced (8, 0);
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
    const int colWidth = row.getWidth() / cols;
    for (int k = 0; k < cols; ++k)
        strip.knobs[(size_t) k].place (row.removeFromLeft (colWidth));
}

void AmpView::resized()
{
    auto area = getLocalBounds().reduced (6, 6);
    if (stereo)
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

// ---------------------------------------------------------------- pedals

PedalPanel::PedalPanel (juce::AudioProcessorValueTreeState& apvts, const Params::PedalDef& d)
    : def (d)
{
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, def.onId, onButton);
    addAndMakeVisible (onButton);

    if (def.toggleId != nullptr)
    {
        toggleButton.setButtonText (def.toggleLabel);
        toggleButton.setClickingTogglesState (true);
        toggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, def.toggleId, toggleButton);
        addAndMakeVisible (toggleButton);
    }

    for (const auto& k : def.knobs)
    {
        knobs.push_back (std::make_unique<KnobUI>());
        knobs.back()->init (*this, apvts, k.id, k.label);
    }
}

void PedalPanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (Palette::panelLight.withAlpha (0.6f));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (Palette::gold.withAlpha (0.8f));
    g.drawRoundedRectangle (b, 10.0f, 1.5f);

    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText (def.title, 18, 10, getWidth() - 190, 30, juce::Justification::centredLeft);
}

void PedalPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    auto header = area.removeFromTop (34);
    onButton.setBounds (header.removeFromRight (74));
    if (def.toggleId != nullptr)
    {
        header.removeFromRight (6);
        toggleButton.setBounds (header.removeFromRight (70).reduced (0, 3));
    }

    area.removeFromTop (4);
    const int colWidth = area.getWidth() / juce::jmax (1, (int) knobs.size());
    for (auto& k : knobs)
        k->place (area.removeFromLeft (colWidth));
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
    auto area = getLocalBounds();
    const int rowHeight = area.getHeight() / juce::jmax (1, (int) rows.size());

    for (const auto& row : rows)
    {
        auto rowArea = area.removeFromTop (rowHeight).reduced (0, 4);

        float total = 0.0f;
        for (int idx : row)
            total += (float) panels[(size_t) idx]->getNumKnobs() + 0.6f;

        for (int idx : row)
        {
            auto& panel = *panels[(size_t) idx];
            const int w = (int) ((float) rowArea.getWidth() * ((float) panel.getNumKnobs() + 0.6f) / total);
            panel.setBounds (rowArea.removeFromLeft (w).reduced (5, 0));
        }
    }
}

// ---------------------------------------------------------------- equalizer

EqView::EqView (juce::AudioProcessorValueTreeState& apvts)
{
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, Params::eqOn, onButton);
    addAndMakeVisible (onButton);

    for (int b = 0; b < Params::numEqBands; ++b)
    {
        auto& band = bands[(size_t) b];
        band.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Params::eqId (b), band.fader);
        band.fader.setDoubleClickReturnValue (true, 0.0);
        band.fader.onValueChange = [this] { updateCurve(); };
        addAndMakeVisible (band.fader);

        band.label.setText (juce::String (Params::eqLabel (b)) + " Hz", juce::dontSendNotification);
        band.label.setJustificationType (juce::Justification::centred);
        band.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        band.label.setColour (juce::Label::textColourId, Palette::amber);
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
    auto area = getLocalBounds().reduced (14, 10);
    onButton.setBounds (area.removeFromTop (30).removeFromLeft (130));
    area.removeFromTop (6);
    curveArea = area.removeFromTop (juce::jmin (150, area.getHeight() / 3));
    area.removeFromTop (8);

    const int colWidth = area.getWidth() / Params::numEqBands;
    for (auto& band : bands)
    {
        auto col = area.removeFromLeft (colWidth);
        band.label.setBounds (col.removeFromTop (22));
        band.fader.setBounds (col.reduced (6, 0));
    }
    updateCurve();
}

void EqView::paint (juce::Graphics& g)
{
    const auto r = curveArea.toFloat();
    g.setColour (Palette::ink.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 6.0f);

    const float centreY = r.getCentreY(), halfH = r.getHeight() * 0.42f;
    const auto xForFreq = [&] (float f) { return r.getX() + std::log (f / 30.0f) / std::log (16000.0f / 30.0f) * r.getWidth(); };

    g.setColour (Palette::muted.withAlpha (0.3f));
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
        g.drawHorizontalLine ((int) (centreY - db / 12.0f * halfH), r.getX(), r.getRight());
    for (int b = 0; b < Params::numEqBands; ++b)
        g.drawVerticalLine ((int) xForFreq (Params::eqFreqs[b]), r.getY(), r.getBottom());

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

MetronomeBar::MetronomeBar (GuitarDriveProcessor& p)
    : proc (p)
{
    onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, Params::metroOn, onButton);
    addAndMakeVisible (onButton);

    bpm.init (*this, p.apvts, Params::metroBpm, "BPM");
    bpm.slider.setWheelFractions (1.0 / 230.0, 1.0 / 230.0);   // one beat per minute per wheel notch
    vol.init (*this, p.apvts, Params::metroVol, "VOLUME");

    for (int i = 0; i < Metronome::numSigs; ++i)
        sigBox.addItem (Metronome::sigName (i), i + 1);
    sigAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, Params::metroSig, sigBox);
    addAndMakeVisible (sigBox);

    sigLabel.setText ("TIME SIG", juce::dontSendNotification);
    sigLabel.setJustificationType (juce::Justification::centred);
    sigLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    sigLabel.setColour (juce::Label::textColourId, Palette::amber);
    addAndMakeVisible (sigLabel);

    tapButton.onClick = [this] { tap(); };
    addAndMakeVisible (tapButton);

    startTimerHz (25);
}

void MetronomeBar::tap()
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

void MetronomeBar::resized()
{
    auto area = getLocalBounds().reduced (14, 6);
    onButton.setBounds (area.removeFromLeft (150).withSizeKeepingCentre (150, 30));
    bpm.place (area.removeFromLeft (100), 16);
    tapButton.setBounds (area.removeFromLeft (84).withSizeKeepingCentre (72, 36));
    area.removeFromLeft (14);

    auto sig = area.removeFromLeft (104);
    sigLabel.setBounds (sig.removeFromTop (16));
    sigBox.setBounds (sig.withSizeKeepingCentre (96, 30));
    area.removeFromLeft (14);

    vol.place (area.removeFromLeft (100), 16);
    area.removeFromLeft (24);
    dotsArea = area.withSizeKeepingCentre (area.getWidth(), 34);
}

void MetronomeBar::paint (juce::Graphics& g)
{
    const int sig = (int) std::lround (proc.apvts.getRawParameterValue (Params::metroSig)->load());
    const int pulses = Metronome::numPulses (sig);
    const int current = proc.getMetronome().getPulse();
    constexpr int d = 20, space = 10;

    for (int i = 0; i < pulses; ++i)
    {
        const auto dot = juce::Rectangle<int> (dotsArea.getX() + i * (d + space), dotsArea.getCentreY() - d / 2, d, d).toFloat();
        const int accent = Metronome::accentFor (sig, i);

        g.setColour (i == current ? (accent == 2 ? Palette::amber : Palette::cream) : Palette::panelLight);
        g.fillEllipse (dot);
        g.setColour (accent == 2 ? Palette::gold : Palette::muted.withAlpha (0.6f));
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
      metroBar (p),
      tunerView (p),
      stereoAttach (*p.apvts.getParameter (GuitarDriveProcessor::stereoId), [this] (float v) { setStereo (v > 0.5f); }),
      rightInAttach (*p.apvts.getParameter (GuitarDriveProcessor::rightSameId),
                     [this] (float v) { rightUsesIn1 = v > 0.5f; rightInButton.setToggleState (rightUsesIn1, juce::dontSendNotification); })
{
    setLookAndFeel (&look);
    setResizable (true, false);
    addAndMakeVisible (root);
    root.painter = [this] (juce::Graphics& g) { paintRoot (g); };

    static const char* const tabNames[4] = { "PRE FX", "AMP", "EQ", "POST FX" };
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

    inputKnob.init (root, p.apvts, Params::inputKnob().id, Params::inputKnob().label);
    outputKnob.init (root, p.apvts, Params::outputKnob().id, Params::outputKnob().label);

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
    root.addAndMakeVisible (metroBar);
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
    designH = headerH + gap + bodyH + gap + footerH + hintH;
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
    metroBar.setVisible (normal);
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
    headerRect = { 214, 8, 538, 68 };
    bodyRect = { 20, headerH + gap, w - 40, bodyH };
    footerRect = { 20, bodyRect.getBottom() + gap, w - 40, footerH };

    for (size_t i = 0; i < tabs.size(); ++i)
        tabs[i].setBounds (224 + (int) i * 88, 24, 84, 36);
    inputKnob.place ({ 590, 8, 72, 68 }, 14);
    outputKnob.place ({ 668, 8, 72, 68 }, 14);

    tunerButton.setBounds (w - 20 - 90, 26, 90, 34);
    stereoButton.setBounds (w - 20 - 90 - 8 - 100, 26, 100, 34);
    rightInButton.setBounds (w - 20 - 90 - 8 - 100 - 8 - 130, 26, 130, 34);

    ampView.setBounds (bodyRect);
    preBoard.setBounds (bodyRect);
    eqView.setBounds (bodyRect);
    postBoard.setBounds (bodyRect);
    metroBar.setBounds (footerRect);
    tunerView.setBounds (bodyRect.withBottom (footerRect.getBottom()));
    repaint();
}

juce::Image GuitarDriveEditor::makeTweed (int w, int h) const
{
    juce::Image img (juce::Image::RGB, w, h, true);
    juce::Graphics g (img);
    g.fillAll (Palette::tweed);

    juce::Random rng (7);
    const juce::Colour threads[] = { Palette::tweedDark.withAlpha (0.55f),
                                     juce::Colour (0xff7d5b2c).withAlpha (0.35f),
                                     juce::Colour (0xffefe0aa).withAlpha (0.5f) };
    for (int y = 0; y < h; y += 3)
        for (int x = rng.nextInt (6); x < w; x += 6 + rng.nextInt (6))
        {
            g.setColour (threads[rng.nextInt (3)]);
            g.fillRect (x, y, 3 + rng.nextInt (3), 1);
        }

    g.setColour (Palette::tweedDark.withAlpha (0.12f));
    for (int x = 0; x < w; x += 4)
        g.drawVerticalLine (x, 0.0f, (float) h);

    const auto addStripe = [&] (int x)
    {
        g.setColour (Palette::stripe);
        g.fillRect (x, 0, 6, h);
        g.setColour (Palette::cream.withAlpha (0.6f));
        g.fillRect (x + 8, 0, 2, h);
    };
    addStripe (2);
    addStripe (w - 12);
    return img;
}

void GuitarDriveEditor::paintRoot (juce::Graphics& g)
{
    if (tweedImage.isNull() || tweedImage.getWidth() != designW || tweedImage.getHeight() != designH)
        tweedImage = makeTweed (designW, designH);
    g.drawImageAt (tweedImage, 0, 0);

    const auto plate = [&g] (juce::Rectangle<int> r, float corner)
    {
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (r.toFloat(), corner);
        g.setColour (Palette::gold);
        g.drawRoundedRectangle (r.toFloat().reduced (1.0f), corner, 2.0f);
        g.setColour (Palette::gold.withAlpha (0.35f));
        g.drawRoundedRectangle (r.toFloat().reduced (5.0f), juce::jmax (2.0f, corner - 3.0f), 1.0f);
    };

    plate (titleRect, 10.0f);
    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("GuitarDrive", titleRect.withTrimmedBottom (14), juce::Justification::centred);
    g.setColour (Palette::gold);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("TWEED SERIES", titleRect.withTrimmedTop (44), juce::Justification::centred);

    plate (headerRect, 10.0f);

    if (tunerOpen)
    {
        plate (bodyRect.withBottom (footerRect.getBottom()), 12.0f);
    }
    else
    {
        plate (bodyRect, 12.0f);
        plate (footerRect, 12.0f);
    }

    g.setColour (Palette::ink);
    g.setFont (juce::FontOptions (12.0f));
    juce::String hint = "Scroll over a knob to turn it  |  Shift = fine  |  Double-click = reset";
    if (stereoMode && view == View::amp && ! tunerOpen)
        hint += "  |  R USES IN 1: on = right strip hears input 1, off = input 2";
    if (tunerOpen)
        hint = "Tuner open: output muted. Click TUNER again, or any tab, to go back";
    g.drawText (hint, 24, designH - hintH, designW - 48, hintH - 2, juce::Justification::centredLeft);
}
