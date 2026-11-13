#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>

// Presets are small XML files in a folder. A preset holds every setting except the metronome (it is a practice
// tool, not part of a sound). The current preset's name is kept in the plugin state, and a snapshot of its values is
// compared with the live settings to tell whether it has been modified since it was loaded or saved.
class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState&, juce::AudioProcessor&, juce::File presetFolder);

    juce::File getFolder() const { return folder; }
    juce::StringArray names() const;                 // sorted
    bool load (const juce::String& name);
    bool save (const juce::String& name);            // stores the current settings; the preset becomes current and clean
    bool remove (const juce::String& name);

    juce::String currentName() const;                // empty when no preset is selected
    bool isDirty() const;                            // current preset differs from what was last loaded or saved
    void restoreFromState();                         // call after the host has restored the plugin state

    static juce::String sanitise (const juce::String& name);

private:
    juce::File fileFor (const juce::String& name) const { return folder.getChildFile (sanitise (name) + ".xml"); }
    juce::Array<juce::RangedAudioParameter*> presetParameters() const;
    void setCurrent (const juce::String& name);
    void captureSnapshot();
    void seedFactoryPresets();

    juce::AudioProcessorValueTreeState& apvts;
    juce::AudioProcessor& processor;
    juce::File folder;
    std::map<juce::String, float> snapshot;   // normalised values by parameter id
};
