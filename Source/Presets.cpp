#include "Presets.h"
#include "Params.h"

namespace
{
    // the metronome and the auto-level setting are not part of a sound
    bool belongsToPreset (const juce::String& id) { return ! id.startsWith ("metro_") && id != Params::autoLevel; }
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

// Factory presets are written the first time the program runs, and rewritten when factoryVersion goes up (new
// parameters, better settings). Presets you made, and factory presets you saved over, are never touched; a factory
// preset you deleted stays deleted until the next version.
void PresetManager::seedFactoryPresets()
{
    constexpr int factoryVersion = 3;
    folder.createDirectory();
    const auto marker = folder.getChildFile (".factory-version");
    if (marker.loadFileAsString().trim().getIntValue() >= factoryVersion)
        return;

    using Values = std::vector<std::pair<const char*, float>>;
    const auto with = [] (Values base, const Values& changes) { base.insert (base.end(), changes.begin(), changes.end()); return base; };

    // Joe Garcia's amps, as set in his Default preset (darker than the knobs at noon: the Fender's treble well down,
    // more bass on the Two-Rock, the Dumble's treble up as it is the darkest). The other presets start from them.
    const Values yourAmps = {
        { "fn_on", 1.0f }, { "fn_volume", 5.6f }, { "fn_treble", 2.9f }, { "fn_mid", 5.0f }, { "fn_bass", 5.9f }, { "fn_master", 7.6f }, { "fn_mic", 10.0f },
        { "rk_on", 1.0f }, { "rk_volume", 5.6f }, { "rk_treble", 5.3f }, { "rk_mid", 5.0f }, { "rk_bass", 7.1f }, { "rk_master", 7.6f }, { "rk_mic", 10.0f },
        { "dm_on", 1.0f }, { "dm_voice", 0.0f }, { "dm_volume", 6.5f }, { "dm_treble", 8.0f }, { "dm_mid", 4.4f }, { "dm_bass", 3.5f }, { "dm_master", 7.6f }, { "dm_mic", 10.0f },
        { "rev_dwell", 5.0f }, { "rev_tone", 5.0f }, { "reverb", 4.2f } };

    struct Factory { const char* name; Values changes; };
    const std::vector<Factory> presets = {
        { "Init", {} },
        // his Default preset, so a new computer starts where he left off (his own copy is never replaced)
        { "Default", with (yourAmps, {
            { "wah_pos", 8.3f }, { "comp_sustain", 6.6f }, { "comp_attack", 8.2f }, { "comp_level", 5.0f },
            { "ts_drive", 3.4f }, { "ts_tone", 4.9f }, { "ts_level", 6.2f }, { "od_drive", 7.6f }, { "od_tone", 5.0f }, { "od_level", 1.7f },
            { "ph_rate", 5.1f }, { "ph_depth", 6.5f }, { "dl_time", 20.0f }, { "dl_mix", 4.8f },
            { "rv_size", 6.8f }, { "rv_damp", 5.6f }, { "rv_mix", 3.6f } }) },
        // John Mayer's clean: three amps at once (he plays a Dumble, a Two-Rock and a Fender together), on the amps above,
        // with light compression for that even, glassy attack. The Tube Screamer is set the way he pushes his amps with
        // one (drive low, level up): press E to kick it in.
        { "Mayer Clean", with (yourAmps, {
            { "comp_on", 1.0f }, { "comp_sustain", 3.5f }, { "comp_attack", 6.0f }, { "comp_level", 5.5f },
            { "ts_on", 0.0f }, { "ts_drive", 3.0f }, { "ts_tone", 5.0f }, { "ts_level", 7.0f } }) },
        // the Dumble alone, through the Two-Rock 2x12
        { "Dumble Klean", with (yourAmps, { { "fn_on", 0.0f }, { "rk_on", 0.0f } }) },
        // Stevie Ray Vaughan: the Fender (his Vibroverbs and Super Reverbs) turned up to the edge of break-up with the
        // mids up, the Dumble under it (he played a Dumble Steel String Singer alongside them), and a Tube Screamer with
        // the drive low and the level way up, hitting the amps hard. A little more spring.
        { "SRV Edge", with (yourAmps, {
            { "rk_on", 0.0f }, { "fn_volume", 6.5f }, { "fn_mid", 6.0f }, { "dm_volume", 6.0f }, { "dm_mid", 5.5f },
            { "ts_on", 1.0f }, { "ts_drive", 3.0f }, { "ts_tone", 5.0f }, { "ts_level", 8.5f },
            { "rev_dwell", 6.0f } }) },
    };

    for (const auto& preset : presets)
    {
        const auto file = fileFor (preset.name);
        if (file.existsAsFile())
        {
            // ours (an older factory version) or from before the three heads: rewrite it; otherwise it's yours: keep it
            const auto old = juce::parseXML (file);
            bool ours = false, hasHeads = false;
            if (old != nullptr)
            {
                ours = old->getIntAttribute ("factory") > 0;
                for (auto* e : old->getChildWithTagNameIterator ("PARAM"))
                    hasHeads = hasHeads || e->getStringAttribute ("id") == "fn_on";
            }
            if (! ours && hasHeads)
                continue;
        }

        juce::XmlElement xml ("PRESET");
        xml.setAttribute ("name", preset.name);
        xml.setAttribute ("version", 1);
        xml.setAttribute ("factory", factoryVersion);

        for (auto* p : presetParameters())
        {
            float value = p->convertFrom0to1 (p->getDefaultValue());
            for (const auto& change : preset.changes)
                if (p->paramID == change.first)
                    value = change.second;

            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", p->paramID);
            e->setAttribute ("value", (double) value);
        }
        xml.writeTo (file);
    }
    marker.replaceWithText (juce::String (factoryVersion));
}
