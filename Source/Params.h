#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Parameter tables shared by the processor (which creates the parameters) and the editor (which builds knobs from them).
namespace Params
{
    struct KnobDef
    {
        const char* id;
        const char* label;
        float min, max, def, step;
        const char* unit = "";
        int decimals = 1;
    };

    struct PedalDef
    {
        const char* title;
        const char* onId;
        std::vector<KnobDef> knobs;
        const char* toggleId = nullptr;      // optional second switch (e.g. wah auto mode)
        const char* toggleLabel = nullptr;
    };

    inline juce::String format (const KnobDef& k, float v)
    {
        return (k.min < 0.0f && v > 0.0f ? "+" : "") + juce::String (v, k.decimals) + k.unit;
    }

    // Knobs that exist once per channel strip (mono / left, and right in stereo mode).
    inline const std::vector<KnobDef>& stripKnobs()
    {
        static const std::vector<KnobDef> defs = {
            { "gate",   "GATE",      0.0f, 10.0f, 0.0f, 0.01f },
            { "trans",  "TRANSPOSE", -12.0f, 12.0f, 0.0f, 1.0f, " st", 0 },
            { "gain",   "GAIN",      0.0f, 10.0f, 5.0f, 0.01f },
            { "warmth", "WARMTH",    0.0f, 10.0f, 3.0f, 0.01f },
            { "bass",   "BASS",      0.0f, 10.0f, 5.0f, 0.01f },
            { "mid",    "MID",       0.0f, 10.0f, 5.0f, 0.01f },
            { "treble", "TREBLE",    0.0f, 10.0f, 5.0f, 0.01f },
            { "reverb", "REVERB",    0.0f, 10.0f, 2.5f, 0.01f },
            { "master", "MASTER",    0.0f, 10.0f, 8.0f, 0.01f },
        };
        return defs;
    }

    // Pedals before the amp, shared by both channels.
    inline const std::vector<PedalDef>& prePedals()
    {
        static const std::vector<PedalDef> defs = {
            { "WAH", "wah_on",
              { { "wah_pos", "POSITION", 0.0f, 10.0f, 5.0f, 0.01f },
                { "wah_sens", "SENS", 0.0f, 10.0f, 5.0f, 0.01f },
                { "wah_q", "RESONANCE", 0.0f, 10.0f, 5.0f, 0.01f } },
              "wah_auto", "AUTO" },
            { "COMPRESSOR", "comp_on",
              { { "comp_sustain", "SUSTAIN", 0.0f, 10.0f, 3.0f, 0.01f },
                { "comp_attack", "ATTACK", 0.0f, 10.0f, 4.0f, 0.01f },
                { "comp_level", "LEVEL", 0.0f, 10.0f, 5.0f, 0.01f } } },
            { "TUBE SCREAMER", "ts_on",
              { { "ts_drive", "DRIVE", 0.0f, 10.0f, 2.5f, 0.01f },   // low drive, extra level: pushes the amp to the edge of breakup
                { "ts_tone", "TONE", 0.0f, 10.0f, 5.5f, 0.01f },
                { "ts_level", "LEVEL", 0.0f, 10.0f, 6.5f, 0.01f } } },
            { "OVERDRIVE", "od_on",
              { { "od_drive", "DRIVE", 0.0f, 10.0f, 4.0f, 0.01f },
                { "od_tone", "TONE", 0.0f, 10.0f, 5.0f, 0.01f },
                { "od_level", "LEVEL", 0.0f, 10.0f, 5.0f, 0.01f } } },
        };
        return defs;
    }

    // Effects after the equalizer, applied to the stereo mix.
    inline const std::vector<PedalDef>& postPedals()
    {
        static const std::vector<PedalDef> defs = {
            { "PHASER", "ph_on",
              { { "ph_rate", "RATE", 0.0f, 10.0f, 3.0f, 0.01f },
                { "ph_depth", "DEPTH", 0.0f, 10.0f, 5.0f, 0.01f },
                { "ph_fb", "FEEDBACK", 0.0f, 10.0f, 3.0f, 0.01f },
                { "ph_mix", "MIX", 0.0f, 10.0f, 5.0f, 0.01f } } },
            { "CHORUS", "ch_on",
              { { "ch_rate", "RATE", 0.0f, 10.0f, 3.0f, 0.01f },
                { "ch_depth", "DEPTH", 0.0f, 10.0f, 5.0f, 0.01f },
                { "ch_mix", "MIX", 0.0f, 10.0f, 5.0f, 0.01f } } },
            { "TREMOLO", "tr_on",
              { { "tr_rate", "RATE", 0.0f, 10.0f, 5.0f, 0.01f },
                { "tr_depth", "DEPTH", 0.0f, 10.0f, 5.0f, 0.01f } } },
            { "DELAY", "dl_on",
              { { "dl_time", "TIME", 20.0f, 1200.0f, 380.0f, 1.0f, " ms", 0 },
                { "dl_fb", "FEEDBACK", 0.0f, 10.0f, 3.0f, 0.01f },
                { "dl_tone", "TONE", 0.0f, 10.0f, 5.0f, 0.01f },
                { "dl_mix", "MIX", 0.0f, 10.0f, 3.0f, 0.01f } } },
            { "DIGITAL REVERB", "rv_on",
              { { "rv_size", "SIZE", 0.0f, 10.0f, 5.0f, 0.01f },
                { "rv_damp", "DAMPING", 0.0f, 10.0f, 5.0f, 0.01f },
                { "rv_mix", "MIX", 0.0f, 10.0f, 3.0f, 0.01f } } },
        };
        return defs;
    }

    // Graphic equalizer
    constexpr int numEqBands = 8;
    inline constexpr const char* eqOn = "eq_on";
    inline const char* eqId (int i) { static const char* ids[numEqBands] = { "eq0", "eq1", "eq2", "eq3", "eq4", "eq5", "eq6", "eq7" }; return ids[i]; }
    inline const char* eqLabel (int i) { static const char* l[numEqBands] = { "62", "125", "250", "500", "1K", "2K", "4K", "8K" }; return l[i]; }
    inline constexpr float eqFreqs[numEqBands] = { 62.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };

    // Plain volume knobs in dB: input before everything, output after everything. They only add amplitude.
    inline const KnobDef& inputKnob()  { static const KnobDef d { "input_vol",  "INPUT",  -24.0f, 24.0f, 0.0f, 0.1f, " dB", 1 }; return d; }
    inline const KnobDef& outputKnob() { static const KnobDef d { "output_vol", "OUTPUT", -48.0f, 12.0f, 0.0f, 0.1f, " dB", 1 }; return d; }

    // Metronome
    inline constexpr const char* metroOn = "metro_on";
    inline constexpr const char* metroBpm = "metro_bpm";
    inline constexpr const char* metroSig = "metro_sig";
    inline constexpr const char* metroVol = "metro_vol";
}
