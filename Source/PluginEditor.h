#pragma once
#include "PluginProcessor.h"
#include "Textures.h"
#include "VintageLook.h"
#include "Widgets.h"

namespace Themes
{
    inline KnobTheme ring() { return { KnobStyle::ring, juce::Colours::white, Palette::text, Palette::text, Palette::silver }; }
}

// A knob with its label, bound to a parameter. The value is shown in a text box under the knob.
struct KnobUI
{
    ScrollKnob slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool labelBelow = false;

    void init (juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
               const juce::String& text, const KnobTheme& theme, bool putLabelBelow = false)
    {
        labelBelow = putLabelBelow;
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
        if (auto* p = apvts.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

        slider.setLook (theme.style, theme.body, theme.scale);
        slider.setColour (juce::Slider::textBoxTextColourId, theme.value);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, theme.label);
        parent.addAndMakeVisible (slider);
        parent.addAndMakeVisible (label);
    }

    void place (juce::Rectangle<int> column, int labelHeight = 22)
    {
        label.setBounds (labelBelow ? column.removeFromBottom (labelHeight) : column.removeFromTop (labelHeight));
        slider.setBounds (column.reduced (2));
    }

    void show (bool visible)
    {
        slider.setVisible (visible);
        label.setVisible (visible);
    }
};

// Ring knob for the top bar: the knob, then its name, then its value.
struct HeaderKnob
{
    ScrollKnob slider;
    juce::Label label, value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    void init (juce::Component& parent, juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& text)
    {
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setLook (KnobStyle::ring, juce::Colours::white, Palette::silver);
        slider.onValueChange = [this] { value.setText (slider.getTextFromValue (slider.getValue()), juce::dontSendNotification); };
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
        if (auto* p = apvts.getParameter (id))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

        for (auto* l : { &label, &value })
        {
            l->setJustificationType (juce::Justification::centred);
            l->setFont (juce::FontOptions (13.0f, juce::Font::bold));
            l->setColour (juce::Label::textColourId, Palette::text);
            parent.addAndMakeVisible (*l);
        }
        label.setText (text, juce::dontSendNotification);
        parent.addAndMakeVisible (slider);
        slider.onValueChange();
    }

    void place (juce::Rectangle<int> r)
    {
        value.setBounds (r.removeFromBottom (20));
        label.setBounds (r.removeFromBottom (22));
        slider.setBounds (r);
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

// The amp fills the window edge to edge: carry handle, tweed cabinet, a control panel divided into sections
// (input, preamp, tone, reverb, master, logo) with cream pointer knobs, and the grille cloth below.
// One amp in mono mode, two (left above right) in stereo mode.
class AmpView : public juce::Component
{
public:
    AmpView (GuitarDriveProcessor&, const juce::Image& logoImage);

    void setStereo (bool on);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    static constexpr int numKnobs = 12;

    struct Strip
    {
        std::array<KnobUI, numKnobs> knobs;
        juce::TextButton revType { "PLATE" };
        int revState = 0;
        juce::TextButton ampType { "FENDER" };
        int ampState = 0;
    };

    struct Group { juce::String title; juce::Rectangle<int> area; };
    struct Layout
    {
        juce::Rectangle<int> cabinet, panel, grille, jack, badge;
        std::vector<Group> groups;
    };

    void layoutAmp (int index, juce::Rectangle<int> area);
    void paintAmp (juce::Graphics&, int index);
    void updateVisibility();

    juce::AudioProcessorValueTreeState& apvts;
    const juce::Image& logo;
    std::array<Strip, 2> strips;
    std::array<std::unique_ptr<juce::ParameterAttachment>, 2> revAttach, ampAttach;
    std::array<Layout, 2> lay;
    juce::Image tweedImage, grilleImage;
    bool stereo = false;
};

// A stompbox floating on the black background: a colored enclosure seen slightly from above, a brushed aluminum
// face with the knobs, the pedal's name in script, and the footswitch with its LED.
class PedalPanel : public juce::Component
{
public:
    PedalPanel (juce::AudioProcessorValueTreeState&, const Params::PedalDef&, int strip);

    int preferredWidth() const { return knobs.size() <= 2 ? 176 : 240; }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Colours { juce::Colour knob, led; };
    static Colours coloursFor (const juce::String& onId);

    const Params::PedalDef& def;
    Colours colours;
    StompSwitch footSwitch;
    std::vector<std::unique_ptr<KnobUI>> knobs;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    juce::Rectangle<int> titleArea, creditArea;
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
    StompSwitch footSwitch { juce::Colour (0xffb56bff) };
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

    StompSwitch footSwitch { juce::Colour (0xff42a9ff) };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
    std::array<Band, Params::numEqBands> bands;
    std::array<juce::dsp::IIR::Coefficients<float>::Ptr, Params::numEqBands> coefs;
    juce::Rectangle<int> curveArea, titleArea;
    juce::Image brushed;
};

// Slim footer under every view: tuner, then the metronome in one row (play/pause, tap, tempo, time signature,
// pulse lights, click volume). Tempo and signature are typed with the keyboard; the wheel and arrow keys nudge them.
class FooterBar : public juce::Component, private juce::Timer
{
public:
    explicit FooterBar (GuitarDriveProcessor&);

    void resized() override;
    void paint (juce::Graphics&) override;

    IconButton tunerButton { Icon::tuner };

private:
    void timerCallback() override { repaint (dotsArea); }
    void tap();

    GuitarDriveProcessor& proc;
    PlayPauseButton playButton;
    NumberBox bpmBox { 30, 260, 3 }, beatsBox { 1, 16, 2 }, noteBox { 2, 16, 2 };
    juce::TextButton tapButton { "TAP" };
    juce::Label slashLabel, clickLabel;
    ScrollKnob clickKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clickAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playAttach;
    juce::ParameterAttachment bpmAttach, beatsAttach, noteAttach;
    std::vector<double> taps;
    juce::Rectangle<int> dotsArea;
};

// Presets, top centre: icons to save, create and delete, then the name between two arrows.
// A modified preset shows an asterisk before its name (like an unsaved tab in a code editor); saving removes it.
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
    IconButton prevButton { Icon::prev }, nextButton { Icon::next }, saveButton { Icon::save }, newButton { Icon::plus }, deleteButton { Icon::trash };
    juce::TextButton nameButton;
    juce::String shownText;
};

class GuitarDriveEditor : public juce::AudioProcessorEditor
{
public:
    explicit GuitarDriveEditor (GuitarDriveProcessor&);
    ~GuitarDriveEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    // for the snapshot tool: 0 wah, 1 pre effects, 2 amp, 3 equalizer, 4 post effects
    void showView (int index) { setView ((View) juce::jlimit (0, 4, index)); }

private:
    enum class View { wah = 0, pre, amp, eq, post };

    // Everything lives in a fixed-size "design" space and is scaled to the window, so the window can be any size.
    struct Root : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    // pan, solo and mute of one channel, shown in the top bar in stereo mode
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

    static constexpr int designW = 1360, topH = 220, footerH = 64, gap = 8;
    static constexpr int monoBodyH = 430, stereoHalfH = 320, sideTagW = 64;

    GuitarDriveProcessor& proc;
    VintageLook look;
    Root root;
    juce::Image logoImage;

    std::array<std::unique_ptr<IconButton>, 5> tabs;
    HeaderKnob inputKnob, outputKnob;
    LevelMeter inputMeter, outputMeter;
    PillSwitch stereoSwitch { "STEREO", "MONO" };
    std::array<ChannelControls, 2> channels;
    juce::TextButton rightInButton { "RIGHT = INPUT 1" };
    AmpView ampView;
    std::array<std::unique_ptr<WahView>, 2> wahViews;
    std::array<std::unique_ptr<PedalBoard>, 2> preBoards, postBoards;
    std::array<std::unique_ptr<EqView>, 2> eqViews;
    FooterBar footer;
    PresetBar presetBar;
    TunerView tunerView;
    juce::ParameterAttachment stereoAttach, rightInAttach, soloAttach;

    juce::Rectangle<int> bodyRect, footerRect;
    int designH = 714, bodyH = monoBodyH, soloState = 0;
    juce::Image backdrop;   // green frame texture, made at the design size
    View view = View::amp;
    bool stereoMode = false, tunerOpen = false, rightUsesIn1 = true, sizeInitialised = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDriveEditor)
};
