#pragma once
#include "Logo.h"
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

// Chromatic tuner shown in place of the views: note name, cents offset and a meter.
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

// The amp, drawn as a tweed combo: control plate with cream Fender-style knobs, and grille cloth with the logo below.
// One amp in mono mode, two (left above right) in stereo mode.
class AmpView : public juce::Component
{
public:
    AmpView (GuitarDriveProcessor&, const juce::Image& logoImage);

    void setStereo (bool on);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    static constexpr int numKnobs = 9;

    struct Strip
    {
        std::array<KnobUI, numKnobs> knobs;
        juce::Label name;
        juce::TextButton revType { "PLATE" };
        int revState = 0;
    };

    void layoutAmp (int index, juce::Rectangle<int> cabinetArea, bool stereoRow);
    void paintAmp (juce::Graphics&, int index);
    void updateVisibility();

    juce::AudioProcessorValueTreeState& apvts;
    const juce::Image& logo;
    std::array<Strip, 2> strips;
    std::array<std::unique_ptr<juce::ParameterAttachment>, 2> revAttach;
    std::array<juce::Rectangle<int>, 2> cabinet, plate, grille;
    juce::Image tweedImage, grilleImage;
    bool stereo = false;
};

// A stompbox shaped like a real one: a tall rectangle of brushed aluminum with the name on top, the knobs below it,
// and the aluminum footswitch with its colored LED at the bottom. `strip` picks the left (0) or right (1) channel.
class PedalPanel : public juce::Component
{
public:
    PedalPanel (juce::AudioProcessorValueTreeState&, const Params::PedalDef&, int strip);

    int getNumKnobs() const { return (int) knobs.size(); }
    int preferredWidth() const { return knobs.size() <= 2 ? 150 : 220; }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Colours { juce::Colour knob, led; };
    static Colours coloursFor (const juce::String& onId);

    const Params::PedalDef& def;
    Colours colours;
    FootSwitch footSwitch;
    std::vector<std::unique_ptr<KnobUI>> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    juce::Rectangle<int> titleArea;
    juce::Image brushed;
};

// Pedals side by side in one row, centred.
class PedalBoard : public juce::Component
{
public:
    PedalBoard (juce::AudioProcessorValueTreeState&, const std::vector<Params::PedalDef>&, int strip);
    void resized() override;

private:
    std::vector<std::unique_ptr<PedalPanel>> panels;
};

// The wah treadle: drag it, scroll over it, or double-click for the middle. Toe down is the top of the picture.
class WahTreadle : public juce::Component
{
public:
    WahTreadle (juce::AudioProcessorValueTreeState&, int strip);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::Rectangle<float> padArea() const;
    void setFromMouse (const juce::MouseEvent&);

    juce::ParameterAttachment attachment;
    float value = 5.0f;
};

// The wah pedal itself: treadle and engage switch.
class WahPedal : public juce::Component
{
public:
    WahPedal (juce::AudioProcessorValueTreeState&, int strip);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WahTreadle treadle;
    FootSwitch footSwitch { juce::Colour (0xffb56bff) };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    juce::Rectangle<int> titleArea;
    juce::Image brushed;
};

// Wah view: the wah pedal and, beside it, the auto-wah box (its switch turns the auto mode on).
class WahView : public juce::Component
{
public:
    WahView (juce::AudioProcessorValueTreeState&, int strip);
    void resized() override;

private:
    WahPedal wah;
    PedalPanel autoWah;
};

// 8-band graphic equalizer pedal with a live response curve.
class EqView : public juce::Component
{
public:
    EqView (juce::AudioProcessorValueTreeState&, int strip);
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

// Footer shown under every view: tuner button, then the metronome in one horizontal row.
// Tempo and time signature are typed with the keyboard (the wheel and arrow keys also nudge them).
class FooterBar : public juce::Component, private juce::Timer
{
public:
    explicit FooterBar (GuitarDriveProcessor&);

    void resized() override;
    void paint (juce::Graphics&) override;

    juce::TextButton tunerButton { "TUNER" };

private:
    void timerCallback() override { repaint (dotsArea); }
    void tap();

    GuitarDriveProcessor& proc;
    PlayPauseButton playButton;
    NumberBox bpmBox { 30, 260, 3 }, beatsBox { 1, 16, 2 }, noteBox { 2, 16, 2 };
    juce::TextButton tapButton { "TAP" };
    juce::Label tempoLabel, sigLabel, slashLabel;
    KnobUI vol;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playAttach;
    juce::ParameterAttachment bpmAttach, beatsAttach, noteAttach;
    std::vector<double> taps;
    juce::Rectangle<int> dotsArea;
};

// Preset bar at the top centre: step through presets, pick one from a menu, save, create and delete.
// A modified preset shows an asterisk after its name (like an unsaved tab in a code editor); saving removes it.
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (GuitarDriveProcessor&);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { refresh(); }
    void refresh();
    void showMenu();
    void step (int direction);
    void saveCurrent();
    void saveAsNew();
    void saveNamed (const juce::String& name);
    void askDelete();
    void askName (const juce::String& title, const juce::String& initial, std::function<void (const juce::String&)> onName);

    GuitarDriveProcessor& proc;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, nameButton, saveButton { "SAVE" }, newButton { "NEW" }, deleteButton { "DELETE" };
    juce::String shownText;
};

class GuitarDriveEditor : public juce::AudioProcessorEditor
{
public:
    explicit GuitarDriveEditor (GuitarDriveProcessor&);
    ~GuitarDriveEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    enum class View { wah = 0, pre, amp, eq, post };

    // Everything lives in a fixed-size "design" space and is scaled to the window, so the window can be any size.
    struct Root : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    // pan, solo and mute of one channel, shown in the header in stereo mode
    struct ChannelControls
    {
        KnobUI pan;
        juce::Label name;
        juce::TextButton solo { "SOLO" }, mute { "MUTE" };
    };

    void paintRoot (juce::Graphics&);
    void recomputeDesign();
    void layout();
    juce::Rectangle<int> channelRect (int channel) const;
    void setStereo (bool);
    void setTunerOpen (bool);
    void setView (View);
    void setSolo (int state);
    void updateSoloButtons();
    void updateVisibility();
    float fitScale() const;
    void keepOnScreen();

    static constexpr int presetBarH = 44, headerH = 104, gap = 8, footerH = 84, hintH = 26, designW = 1360;
    static constexpr int monoBodyH = 340, stereoHalfH = 330, sideTagW = 64;

    GuitarDriveProcessor& proc;
    VintageLook look;
    Root root;
    juce::Image logoImage;

    std::array<juce::TextButton, 5> tabs;
    KnobUI inputKnob, outputKnob;
    std::array<ChannelControls, 2> channels;
    juce::TextButton stereoButton { "STEREO" }, rightInButton { "R USES IN 1" };
    AmpView ampView;
    std::array<std::unique_ptr<WahView>, 2> wahViews;
    std::array<std::unique_ptr<PedalBoard>, 2> preBoards, postBoards;
    std::array<std::unique_ptr<EqView>, 2> eqViews;
    FooterBar footer;
    PresetBar presetBar;
    TunerView tunerView;
    juce::ParameterAttachment stereoAttach, rightInAttach, soloAttach;

    juce::Image frameImage, carpetImage;
    juce::Rectangle<int> titleRect, headerRect, channelPlateRect, bodyRect, footerRect;
    int designH = 550, bodyH = monoBodyH, soloState = 0;
    View view = View::amp;
    bool stereoMode = false, tunerOpen = false, rightUsesIn1 = true, sizeInitialised = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDriveEditor)
};
