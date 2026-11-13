#include "Presets.h"

namespace
{
    bool belongsToPreset (const juce::String& id) { return ! id.startsWith ("metro_"); }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state, juce::AudioProcessor& owner, juce::File presetFolder)
    : apvts (state), processor (owner), folder (std::move (presetFolder))
{
    seedFactoryPresets();
}

juce::String PresetManager::sanitise (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).trim();
}

juce::Array<juce::RangedAudioParameter*> PresetManager::presetParameters() const
{
    juce::Array<juce::RangedAudioParameter*> out;
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (belongsToPreset (ranged->paramID))
                out.add (ranged);
    return out;
}

juce::StringArray PresetManager::names() const
{
    juce::StringArray result;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.xml"))
        result.add (f.getFileNameWithoutExtension());
    result.sortNatural();
    return result;
}

juce::String PresetManager::currentName() const
{
    return apvts.state.getProperty ("preset").toString();
}

void PresetManager::setCurrent (const juce::String& name)
{
    apvts.state.setProperty ("preset", name, nullptr);
}

void PresetManager::captureSnapshot()
{
    snapshot.clear();
    for (auto* p : presetParameters())
        snapshot[p->paramID] = p->getValue();
}

bool PresetManager::isDirty() const
{
    if (currentName().isEmpty() || snapshot.empty())
        return false;

    for (auto* p : presetParameters())
    {
        const auto it = snapshot.find (p->paramID);
        if (it != snapshot.end() && std::abs (p->getValue() - it->second) > 1.0e-4f)
            return true;
    }
    return false;
}

bool PresetManager::save (const juce::String& name)
{
    const auto clean = sanitise (name);
    if (clean.isEmpty() || ! folder.createDirectory())
        return false;

    juce::XmlElement xml ("PRESET");
    xml.setAttribute ("name", clean);
    xml.setAttribute ("version", 1);
    for (auto* p : presetParameters())
    {
        auto* e = xml.createNewChildElement ("PARAM");
        e->setAttribute ("id", p->paramID);
        e->setAttribute ("value", (double) p->convertFrom0to1 (p->getValue()));
    }

    if (! xml.writeTo (fileFor (clean)))
        return false;

    setCurrent (clean);
    captureSnapshot();
    return true;
}

bool PresetManager::load (const juce::String& name)
{
    const auto xml = juce::parseXML (fileFor (name));
    if (xml == nullptr || ! xml->hasTagName ("PRESET"))
        return false;

    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = e->getStringAttribute ("id");
        if (! belongsToPreset (id))
            continue;

        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) e->getDoubleAttribute ("value")));
            p->endChangeGesture();
        }
    }

    setCurrent (sanitise (name));
    captureSnapshot();
    return true;
}

bool PresetManager::remove (const juce::String& name)
{
    const bool deleted = fileFor (name).deleteFile();
    if (sanitise (name) == currentName())
    {
        setCurrent ({});
        snapshot.clear();
    }
    return deleted;
}

void PresetManager::restoreFromState()
{
    const auto name = currentName();
    if (name.isEmpty())
        return;

    const auto xml = juce::parseXML (fileFor (name));
    if (xml == nullptr || ! xml->hasTagName ("PRESET"))
    {
        setCurrent ({});   // the preset file is gone
        snapshot.clear();
        return;
    }

    // what the preset holds, so that edits made before the project was saved still show as modified
    captureSnapshot();
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        if (auto* p = apvts.getParameter (e->getStringAttribute ("id")))
            if (belongsToPreset (p->paramID))
                snapshot[p->paramID] = p->convertTo0to1 ((float) e->getDoubleAttribute ("value"));
}

// Written once, the first time the program runs. Deleting them later sticks.
void PresetManager::seedFactoryPresets()
{
    if (folder.exists())
        return;
    folder.createDirectory();

    struct Factory { const char* name; std::vector<std::pair<const char*, float>> changes; };
    const std::vector<Factory> presets = {
        { "Init", {} },
        // Two-Rock for headroom, light compression, spring reverb
        { "Mayer Clean", { { "amptype", 1.0f }, { "comp_on", 1.0f }, { "comp_sustain", 3.5f }, { "comp_level", 5.5f },
                           { "gain", 4.5f }, { "bass", 5.0f }, { "mid", 4.5f }, { "treble", 6.0f },
                           { "reverb", 3.0f }, { "revtype", 1.0f } } },
        // Tube Screamer with low drive and the level up, pushing a Deluxe at the edge of breakup
        { "SRV Edge",    { { "ts_on", 1.0f }, { "ts_drive", 2.0f }, { "ts_tone", 6.0f }, { "ts_level", 8.0f },
                           { "gain", 5.0f }, { "warmth", 4.0f }, { "bass", 5.5f }, { "mid", 5.5f }, { "treble", 5.5f },
                           { "reverb", 2.5f }, { "revtype", 1.0f } } },
        { "Deluxe Breakup", { { "gain", 6.5f }, { "warmth", 5.0f }, { "treble", 5.5f }, { "reverb", 2.5f }, { "revtype", 1.0f } } },
    };

    for (const auto& preset : presets)
    {
        juce::XmlElement xml ("PRESET");
        xml.setAttribute ("name", preset.name);
        xml.setAttribute ("version", 1);

        for (auto* p : presetParameters())
        {
            float value = p->convertFrom0to1 (p->getDefaultValue());
            for (const auto& change : preset.changes)   // applies to the right channel's copy too
                if (p->paramID == change.first || p->paramID == "r_" + juce::String (change.first))
                    value = change.second;

            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", p->paramID);
            e->setAttribute ("value", (double) value);
        }
        xml.writeTo (fileFor (preset.name));
    }
}
