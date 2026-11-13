#pragma once
#include "PluginProcessor.h"
#include "Textures.h"
#include "VintageLook.h"

// A knob with its label, bound to a parameter.
struct KnobUI
{
    ScrollKnob slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    void init (juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
               const juce::String& text, const KnobTheme& theme)
    {
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
        if (auto* p = apvts.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

        slider.setLook (theme.style, theme.body, theme.scale);
        slider.setColour (juce::Slider::textBoxTextColourId, theme.value);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, theme.label);
        parent.addAndMakeVisible (slider);
        parent.addAndMakeVisible (label);
    }

    void place (juce::Rectangle<int> column, int labelHeight = 22)
    {
        label.setBounds (column.removeFromTop (labelHeight));
        slider.setBounds (column.reduced (4));
    }

    void show (bool visible)
    {
        slider.setVisible (visible);
        label.setVisible (visible);
    }
};

// Chromatic tuner shown in place of everything else: note name, cents offset and a meter.
class TunerView : public juce::Component, private juce::Timer
{
public:
    explicit TunerView (GuitarDriveProcessor&);

    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void resetReading();

    GuitarDriveProcessor& proc;
    PitchDetector detector;
    std::vector<float> frame;

    std::array<float, 5> history {};
    int histCount = 0, histPos = 0;
    double lastValidMs = 0.0;

    bool haveNote = false;
    int note = 0, shownNote = -1000;
    float cents = 0.0f, displayCents = 0.0f;
    double hz = 0.0;
};

// The amp, drawn as a tweed combo: control plate with chrome knobs, and grille cloth below.
// One amp in mono mode, two (left and right) in stereo mode.
class AmpView : public juce::Component
{
public:
    explicit AmpView (GuitarDriveProcessor&);

    void setStereo (bool on);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    static constexpr int numKnobs = 10;   // the 9 strip knobs plus pan

    struct Strip
    {
        std::array<KnobUI, numKnobs> knobs;
        juce::Label name;
        juce::TextButton solo { "SOLO" }, mute { "MUTE" }, revType { "PLATE" };
        int revState = 0;
    };

    void layoutAmp (int index, juce::Rectangle<int> cabinetArea, bool stereoRow);
    void paintAmp (juce::Graphics&, int index);
    void setSolo (int state);
    void updateSoloButtons();
    void updateVisibility();

    juce::AudioProcessorValueTreeState& apvts;
    std::array<Strip, 2> strips;
    std::array<std::unique_ptr<juce::ParameterAttachment>, 2> revAttach;
    juce::ParameterAttachment soloAttach;
    std::array<juce::Rectangle<int>, 2> cabinet, plate, grille;
    juce::Image tweedImage, grilleImage;
    bool stereo = false;
    int soloState = 0;
};

// One stompbox: brushed aluminum, cream Strat knobs, and a footswitch with a colored LED.
class PedalPanel : public juce::Component
{
public:
    PedalPanel (juce::AudioProcessorValueTreeState&, const Params::PedalDef&);

    int getNumKnobs() const { return (int) knobs.size(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Colours { juce::Colour knob, led; };
    static Colours coloursFor (const juce::String& onId);

    const Params::PedalDef& def;
    Colours colours;
    FootSwitch footSwitch;
    juce::TextButton toggleButton;
    std::vector<std::unique_ptr<KnobUI>> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach, toggleAttach;
    juce::Rectangle<int> titleArea;
    juce::Image brushed;
};

// Several pedals laid out in rows; `rows` lists the pedal indices on each row.
class PedalBoard : public juce::Component
{
public:
    PedalBoard (juce::AudioProcessorValueTreeState&, const std::vector<Params::PedalDef>&, std::vector<std::vector<int>> rows);
    void resized() override;

private:
    std::vector<std::unique_ptr<PedalPanel>> panels;
    std::vector<std::vector<int>> rows;
};

// 8-band graphic equalizer pedal with a live response curve.
class EqView : public juce::Component
{
public:
    explicit EqView (juce::AudioProcessorValueTreeState&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void updateCurve();

    struct Band
    {
        ScrollFader fader;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    FootSwitch footSwitch { juce::Colour (0xff42a9ff) };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    std::array<Band, Params::numEqBands> bands;
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, Params::numEqBands> coefs;
    juce::Rectangle<int> curveArea, titleArea;
    juce::Image brushed;
};

// The metronome as its own device: big BPM readout, tap tempo, time signature buttons and pulse lights.
class MetronomeView : public juce::Component, private juce::Timer
{
public:
    explicit MetronomeView (GuitarDriveProcessor&);

    void resized() override;
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override { repaint (displayArea); }
    void tap();

    GuitarDriveProcessor& proc;
    KnobUI bpm, vol;
    FootSwitch runSwitch { juce::Colour (0xff3dff6c) };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAttach;
    juce::TextButton tapButton { "TAP" };
    std::array<juce::TextButton, Metronome::numSigs> sigButtons;
    juce::Label sigLabel, runLabel, tapLabel;
    juce::ParameterAttachment sigAttach;
    std::vector<double> taps;
    juce::Rectangle<int> displayArea;
    int sigIndex = 2;
};

class GuitarDriveEditor : public juce::AudioProcessorEditor
{
public:
    explicit GuitarDriveEditor (GuitarDriveProcessor&);
    ~GuitarDriveEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    enum class View { pre = 0, amp, eq, post, metro };

    // Everything lives in a fixed-size "design" space and is scaled to the window, so the window can be any size.
    struct Root : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    void paintRoot (juce::Graphics&);
    void recomputeDesign();
    void layout();
    void setStereo (bool);
    void setTunerOpen (bool);
    void setView (View);
    void updateVisibility();
    float fitScale() const;
    void keepOnScreen();

    static constexpr int headerH = 84, gap = 8, hintH = 26, designW = 1200;

    GuitarDriveProcessor& proc;
    VintageLook look;
    Root root;

    std::array<juce::TextButton, 5> tabs;
    KnobUI inputKnob, outputKnob;
    juce::TextButton tunerButton { "TUNER" }, stereoButton { "STEREO" }, rightInButton { "R USES IN 1" };
    AmpView ampView;
    PedalBoard preBoard, postBoard;
    EqView eqView;
    MetronomeView metroView;
    TunerView tunerView;
    juce::ParameterAttachment stereoAttach, rightInAttach;

    juce::Image frameImage, carpetImage;
    juce::Rectangle<int> titleRect, headerRect, bodyRect;
    int designH = 460, bodyH = 340;
    View view = View::amp;
    bool stereoMode = false, tunerOpen = false, rightUsesIn1 = true, sizeInitialised = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDriveEditor)
};
