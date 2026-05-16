#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/Modulation/ModNames.h"
#include "Engine/Modulation/StepSequencerMod.h"
#include <cmath>
#include <optional>

using APVTS = juce::AudioProcessorValueTreeState;
using RAP   = juce::RangedAudioParameter;

static constexpr float kEnvTimeMin  = 0.001f;
static constexpr float kEnvTimeMax  = 60.0f;
static constexpr float kEnvTimeSkew = 0.25f;

//==============================================================================
static APVTS::ParameterLayout buildLayout()
{
    std::vector<std::unique_ptr<RAP>> params;

    auto addFloat = [&](const juce::String& id, const juce::String& name,
                        float min, float max, float def,
                        float skew = 1.0f)
    {
        auto range = juce::NormalisableRange<float>(min, max, 0.0f, skew);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, 1}, name, range, def));
    };

    auto addInt = [&](const juce::String& id, const juce::String& name,
                      int min, int max, int def)
    {
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{id, 1}, name, min, max, def));
    };

    auto addBool = [&](const juce::String& id, const juce::String& name, bool def)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{id, 1}, name, def));
    };

    auto addChoice = [&](const juce::String& id, const juce::String& name,
                         juce::StringArray choices, int def)
    {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{id, 1}, name, choices, def));
    };

    auto addEnvTime = [&](const juce::String& id, const juce::String& name, float def)
    {
        addFloat(id, name, kEnvTimeMin, kEnvTimeMax, def, kEnvTimeSkew);
    };

    // ── Master ────────────────────────────────────────────────────────────────
    addFloat("master_volume",  "Master Volume",  0.0f, 1.0f, 0.8f, 0.5f);
    addFloat("master_tune",    "Master Tune",   -100.0f, 100.0f, 0.0f);
    addInt  ("master_octave",  "Master Octave", -3, 3, 0);
    addFloat("master_pan",     "Master Pan",    -1.0f, 1.0f, 0.0f);

    // Master ADSR
    addEnvTime("master_adsr_a", "Master Attack",  0.01f);
    addEnvTime("master_adsr_d", "Master Decay",   0.2f);
    addFloat("master_adsr_s", "Master Sustain",  0.0f,   1.0f,  0.8f);
    addEnvTime("master_adsr_r", "Master Release", 0.3f);

    addFloat("master_portamento", "Portamento Time",  0.0f, 5.0f,  0.0f, 0.4f);
    addFloat("master_age",        "Age",              0.0f, 1.0f,  0.0f);
    addFloat("master_feedback",   "Feedback",         0.0f, 0.95f, 0.0f);
    addFloat("master_pb_range",   "Pitch Bend Range", 0.0f, 24.0f, 2.0f);
    addFloat("master_mw_range",   "Mod Wheel Range",  0.0f, 1.0f,  0.5f);

    auto indexedPrefix = [](const juce::String& base, int index)
    {
        return index == 0 ? juce::String("osc_") + base + "_"
                          : juce::String("osc_") + base + juce::String(index + 1) + "_";
    };

    // ── 3D Oscillators ────────────────────────────────────────────────────────
    auto add3DOsc = [&](int index, const juce::String& prefix)
    {
        juce::String label = "3D" + juce::String(index + 1);
        addInt  (prefix + "field",    label + " Field",    0, 17, 0);
        addInt  (prefix + "path",     label + " Path",     0, 4, 0);
        addFloat(prefix + "radius",   label + " Radius",   0.1f, 4.0f, 1.0f);
        addFloat(prefix + "depth",    label + " Depth",    0.0f, 4.0f, 1.0f);
        addFloat(prefix + "warp",     label + " Warp",     0.0f, 2.0f, 0.25f);
        addFloat(prefix + "rotation", label + " Rotation", 0.0f, 1.0f, 0.0f);
        addFloat(prefix + "chaos",    label + " Chaos",    0.0f, 1.0f, 0.0f);
        addFloat(prefix + "morph",    label + " Morph",    0.0f, 1.0f, 0.0f);
        addFloat(prefix + "field_freq", label + " Field Freq", 0.25f, 16.0f, 1.0f, 0.4f);
        addFloat(prefix + "z_ratio",    label + " Z Ratio",    0.25f, 4.0f,  1.0f, 0.7f);
        addFloat(prefix + "fold",       label + " Fold",       0.0f,  1.0f,  0.0f);
        addFloat(prefix + "drive",      label + " Drive",      0.25f, 4.0f,  1.0f, 0.5f);
        addBool (prefix + "on",       label + " Enable",   true);
        addFloat(prefix + "level",    label + " Level",    0.0f, 1.0f, 0.8f);
        addFloat(prefix + "octave",   label + " Octave",   -3.0f, 3.0f, 0.0f);
        addEnvTime(prefix + "adsr_a", label + " Env Attack",  0.01f);
        addEnvTime(prefix + "adsr_d", label + " Env Decay",   0.2f);
        addFloat(prefix + "adsr_s",   label + " Env Sustain", 0.0f, 1.0f, 1.0f);
        addEnvTime(prefix + "adsr_r", label + " Env Release", 0.3f);
    };

    for (int i = 0; i < 8; ++i)
        add3DOsc(i, indexedPrefix("3d", i));

    // ── FM Engines ────────────────────────────────────────────────────────────
    juce::StringArray opWaveforms {
        "Sine","Triangle","Saw","Square","Pulse","Noise",
        "All Wide","All Narrow","Odd Wide","Odd Narrow","Res Wide","Res Narrow"
    };

    auto addFMEngine = [&](int engineIndex, const juce::String& fmPrefix,
                           const juce::String& opBase, float defaultLevel)
    {
        juce::String label = "FM " + juce::String(engineIndex == 0 ? "A" : "B");
        addBool (fmPrefix + "on",        label + " Enable",    true);
        addInt  (fmPrefix + "algorithm", label + " Algorithm", 0, 39, 0);
        addFloat(fmPrefix + "level",     label + " Level",     0.0f, 1.0f, defaultLevel);
        addFloat(fmPrefix + "octave",    label + " Octave",   -3.0f, 3.0f, 0.0f);
        addFloat(fmPrefix + "detune",    label + " Master Detune", -100.0f, 100.0f, 0.0f);
        addFloat(fmPrefix + "phase",     label + " Phase Position", 0.0f, 1.0f, 0.0f);
        addFloat(fmPrefix + "algo_morph", label + " Algorithm Morph", 0.0f, 1.0f, 0.0f);
        addFloat(fmPrefix + "index",     label + " Mod Index Scale", 0.0f, 2.0f, 1.0f);
        addEnvTime(fmPrefix + "adsr_a",  label + " Env Attack",  0.01f);
        addEnvTime(fmPrefix + "adsr_d",  label + " Env Decay",   0.2f);
        addFloat(fmPrefix + "adsr_s",    label + " Env Sustain", 0.0f, 1.0f, 1.0f);
        addEnvTime(fmPrefix + "adsr_r",  label + " Env Release", 0.3f);

        for (int i = 0; i < 10; ++i)
        {
            juce::String prefix = opBase + juce::String(i) + "_";
            juce::String opName = label + " Op" + juce::String(i);
            addInt   (prefix + "coarse",   opName + " Coarse",   0,  31, 1);
            addFloat (prefix + "fine",     opName + " Fine",    -1.0f, 1.0f, 0.0f);
            addFloat (prefix + "detune",   opName + " Detune", -15.0f, 15.0f, 0.0f);
            addFloat (prefix + "level",    opName + " Level",    0.0f, 1.0f, i == 0 ? 1.0f : 0.5f);
            addFloat (prefix + "feedback", opName + " Feedback", 0.0f, 1.0f, 0.0f);
            addBool  (prefix + "fixed",    opName + " Fixed Freq", false);
            addChoice(prefix + "wave",     opName + " Wave",     opWaveforms, 0);
            addFloat (prefix + "skirt",    opName + " Spectral Skirt", 0.0f, 1.0f, 0.35f);
            addFloat (prefix + "resonance",opName + " Spectral Resonance", 0.0f, 1.0f, 0.25f);
            addBool  (prefix + "key_reset",opName + " Key On Reset", true);
            addFloat (prefix + "vel",      opName + " Vel Depth",   0.0f, 1.0f, 0.0f);
            addFloat (prefix + "key",      opName + " Key Scale",  -1.0f, 1.0f, 0.0f);
            addEnvTime(prefix + "adsr_a",  opName + " Attack",  0.01f);
            addEnvTime(prefix + "adsr_d",  opName + " Decay",   0.5f);
            addFloat (prefix + "adsr_s",   opName + " Sustain", 0.0f, 1.0f, 0.8f);
            addEnvTime(prefix + "adsr_r",  opName + " Release", 0.3f);
        }
    };

    addFMEngine(0, "osc_fm_",  "op",   0.75f);
    addFMEngine(1, "osc_fm2_", "op2_", 0.75f);

    // ── Wavetable Oscillators ────────────────────────────────────────────────
    auto addWTOsc = [&](int index, const juce::String& prefix)
    {
        juce::String label = "WT" + juce::String(index + 1);
        addBool (prefix + "on",    label + " Enable",   true);
        addChoice(prefix + "table", label + " Table",
                  {"Sine","Triangle","Saw","Square","Pulse","Half Sine","Abs Sine","Sine Cube",
                   "Hollow Odd","Even Glass","Vocal A","Metallic","Sync Saw","Reso Fold","Noisy Digital"}, 0);
        addChoice(prefix + "warp_mode", label + " Warp Mode",
                  {"Off","Bend+","Bend-","Bend +/-","Mirror","Sync","Window Sync",
                   "Quantize","Fold","Formant","PD Saw","PD Square","PD Pulse",
                   "PD Resonant","PD Formant"}, 0);
        addFloat(prefix + "pos",        label + " Position",       0.0f, 1.0f, 0.0f);
        addFloat(prefix + "warp",       label + " Warp Amount",    0.0f, 1.0f, 0.0f);
        addFloat(prefix + "pd_break",   label + " PD Breakpoint",  0.0f, 1.0f, 0.75f);
        addInt  (prefix + "unison",     label + " Unison",         1, 8, 1);
        addFloat(prefix + "detune",     label + " Detune",         0.0f, 1.0f, 0.0f);
        addFloat(prefix + "spread",     label + " Stereo Spread",  0.0f, 1.0f, 0.0f);
        addFloat(prefix + "pos_spread", label + " Position Spread",0.0f, 1.0f, 0.0f);
        addFloat(prefix + "phase",      label + " Phase Offset",   0.0f, 1.0f, 0.0f);
        addFloat(prefix + "random",     label + " Random Phase",   0.0f, 1.0f, 0.0f);
        addBool (prefix + "retrig",     label + " Retrigger",      true);
        addFloat(prefix + "level",      label + " Level",          0.0f, 1.0f, 0.0f);
        addFloat(prefix + "octave",     label + " Octave",        -3.0f, 3.0f, 0.0f);
        addEnvTime(prefix + "adsr_a", label + " Env Attack",  0.01f);
        addEnvTime(prefix + "adsr_d", label + " Env Decay",   0.2f);
        addFloat(prefix + "adsr_s", label + " Env Sustain", 0.0f, 1.0f, 1.0f);
        addEnvTime(prefix + "adsr_r", label + " Env Release", 0.3f);
    };

    for (int i = 0; i < 8; ++i)
        addWTOsc(i, indexedPrefix("wt", i));

    // ── Granular Oscillators ─────────────────────────────────────────────────
    auto addGranOsc = [&](int index, const juce::String& prefix)
    {
        juce::String label = "Granular" + juce::String(index + 1);
        addBool (prefix + "on",    label + " Enable", true);
        addChoice(prefix + "trigger_mode", label + " Trigger Mode",
                  {"Free Cloud","Burst","Audio Rate"}, 0);
        addChoice(prefix + "window", label + " Window",
                  {"Hann","Triangle","Gaussian","Tukey","Rect","Exp Attack","Exp Decay"}, 0);
        addFloat(prefix + "position", label + " Position", 0.0f, 1.0f, 0.5f);
        addFloat(prefix + "size",     label + " Size",     5.0f, 1000.0f, 120.0f, 0.35f);
        addFloat(prefix + "density",  label + " Density",  0.1f, 1200.0f, 35.0f, 0.32f);
        addFloat(prefix + "pitch",    label + " Pitch",   -48.0f, 48.0f, 0.0f);
        addFloat(prefix + "spray",    label + " Spray",    0.0f, 1.0f, 0.18f);
        addFloat(prefix + "jitter",   label + " Jitter",   0.0f, 1.0f, 0.04f);
        addFloat(prefix + "pan",      label + " Pan Spread", 0.0f, 1.0f, 0.7f);
        addFloat(prefix + "reverse",  label + " Reverse",  0.0f, 1.0f, 0.0f);
        addFloat(prefix + "scan",     label + " Scan",    -1.0f, 1.0f, 0.0f);
        addFloat(prefix + "freeze",   label + " Freeze",   0.0f, 1.0f, 0.0f);
        addChoice(prefix + "body_type", label + " Body Type",
                  {"Off","Glass","Metal","Wood","Plate","String","Drum","Pipe","Alien"}, 0);
        addFloat(prefix + "body_root",   label + " Body Root",   -48.0f, 48.0f, 0.0f);
        addFloat(prefix + "body_size",   label + " Body Size",    0.0f, 1.0f, 0.5f);
        addFloat(prefix + "body_decay",  label + " Body Decay",   0.0f, 1.0f, 0.5f);
        addFloat(prefix + "body_damp",   label + " Body Damping", 0.0f, 1.0f, 0.35f);
        addFloat(prefix + "body_bright", label + " Body Bright",  0.0f, 1.0f, 0.6f);
        addFloat(prefix + "body_inharm", label + " Body Inharm",  0.0f, 1.0f, 0.2f);
        addFloat(prefix + "body_excite", label + " Body Excite",  0.0f, 1.0f, 0.7f);
        addFloat(prefix + "body_mix",    label + " Body Mix",     0.0f, 1.0f, 0.0f);
        addFloat(prefix + "body_spread", label + " Body Spread",  0.0f, 1.0f, 0.5f);
        addInt  (prefix + "body_modes",  label + " Body Modes",   4, 32, 16);
        addFloat(prefix + "level",    label + " Level", 0.0f, 1.0f, 0.0f);
        addFloat(prefix + "octave",   label + " Octave",-3.0f, 3.0f, 0.0f);
        addEnvTime(prefix + "adsr_a", label + " Env Attack",  0.01f);
        addEnvTime(prefix + "adsr_d", label + " Env Decay",   0.2f);
        addFloat(prefix + "adsr_s",   label + " Env Sustain", 0.0f, 1.0f, 1.0f);
        addEnvTime(prefix + "adsr_r", label + " Env Release", 0.4f);
    };

    for (int i = 0; i < 8; ++i)
        addGranOsc(i, indexedPrefix("grain", i));

    // ── Wave-Terrain Oscillators ─────────────────────────────────────────────
    auto addTerrainOsc = [&](int index, const juce::String& prefix)
    {
        juce::String label = "Terrain" + juce::String(index + 1);
        addBool  (prefix + "on",        label + " Enable", true);
        addChoice(prefix + "path",      label + " Path Mode",
                  {"Circle","Ellipse","Lissajous","Spiral","Line","Orbit Noise"}, 0);
        addFloat (prefix + "center_x",  label + " Center X", 0.0f, 1.0f, 0.5f);
        addFloat (prefix + "center_y",  label + " Center Y", 0.0f, 1.0f, 0.5f);
        addFloat (prefix + "radius_x",  label + " Radius X", 0.0f, 0.5f, 0.35f);
        addFloat (prefix + "radius_y",  label + " Radius Y", 0.0f, 0.5f, 0.35f);
        addFloat (prefix + "ratio_x",   label + " Ratio X",  0.125f, 8.0f, 1.0f, 0.55f);
        addFloat (prefix + "ratio_y",   label + " Ratio Y",  0.125f, 8.0f, 1.5f, 0.55f);
        addFloat (prefix + "scan_rate", label + " Scan Rate",0.05f, 8.0f, 1.0f, 0.45f);
        addFloat (prefix + "phase",     label + " Phase",    0.0f, 1.0f, 0.0f);
        addFloat (prefix + "spiral",    label + " Spiral",   0.0f, 1.0f, 0.0f);
        addFloat (prefix + "drive",     label + " Drive",    0.1f, 8.0f, 1.5f, 0.45f);
        addFloat (prefix + "offset",    label + " Offset",  -1.0f, 1.0f, 0.0f);
        addFloat (prefix + "smoothing", label + " Smoothing",0.0f, 1.0f, 0.0f);
        addFloat (prefix + "morph",     label + " Morph",    0.0f, 1.0f, 0.0f);
        addFloat (prefix + "gain",      label + " Gain",     0.0f, 2.0f, 1.0f);
        addFloat (prefix + "level",     label + " Level",    0.0f, 1.0f, 0.0f);
        addFloat (prefix + "octave",    label + " Octave",  -3.0f, 3.0f, 0.0f);
        addEnvTime(prefix + "adsr_a",   label + " Env Attack",  0.01f);
        addEnvTime(prefix + "adsr_d",   label + " Env Decay",   0.2f);
        addFloat  (prefix + "adsr_s",   label + " Env Sustain", 0.0f, 1.0f, 1.0f);
        addEnvTime(prefix + "adsr_r",   label + " Env Release", 0.3f);
    };

    for (int i = 0; i < 8; ++i)
        addTerrainOsc(i, indexedPrefix("terrain", i));

    // ── Virtual Analog Oscillators ────────────────────────────────────────────
    auto addVAOsc = [&](int index, const juce::String& prefix, bool enabledByDefault)
    {
        juce::String label = "VA" + juce::String(index);
        addBool (prefix + "on",      label + " Enable", enabledByDefault);
        addFloat(prefix + "shape",   label + " Shape Morph", 0.0f, 5.0f, 1.0f);
        addFloat(prefix + "pw",      label + " Pulse Width", 0.05f, 0.95f, 0.5f);
        addFloat(prefix + "detune",  label + " Detune",      0.0f, 1.0f, 0.0f);
        addInt  (prefix + "unison",  label + " Unison",      1, 8, 1);
        addFloat(prefix + "spread",  label + " Spread",      0.0f, 1.0f, 0.5f);
        addFloat(prefix + "ring",    label + " Ring Mod",    0.0f, 1.0f, 0.0f);
        addFloat(prefix + "fold",    label + " Wave Fold",   0.0f, 1.0f, 0.0f);
        addFloat(prefix + "level",   label + " Level",       0.0f, 1.0f, 0.0f);
        addFloat(prefix + "octave",  label + " Octave",     -3.0f, 3.0f, 0.0f);
        addEnvTime(prefix + "adsr_a", label + " Env Attack",  0.01f);
        addEnvTime(prefix + "adsr_d", label + " Env Decay",   0.2f);
        addFloat(prefix + "adsr_s",  label + " Env Sustain", 0.0f, 1.0f, 1.0f);
        addEnvTime(prefix + "adsr_r", label + " Env Release", 0.3f);
    };

    addVAOsc(1, "osc_va_",  true);
    addVAOsc(2, "osc_va2_", true);
    addVAOsc(3, "osc_va3_", true);
    addVAOsc(4, "osc_va4_", true);
    addVAOsc(5, "osc_va5_", true);
    addVAOsc(6, "osc_va6_", true);
    addVAOsc(7, "osc_va7_", true);
    addVAOsc(8, "osc_va8_", true);

    // ── Karplus-Strong Oscillators ───────────────────────────────────────────
    auto addKSOsc = [&](int index, const juce::String& prefix)
    {
        juce::String label = "KS" + juce::String(index + 1);
        addBool  (prefix + "on",         label + " Enable",    true);
        addFloat (prefix + "level",      label + " Level",     0.0f, 1.0f, 0.0f);
        addFloat (prefix + "octave",     label + " Octave",   -3.0f, 3.0f, 0.0f);
        addFloat (prefix + "decay",      label + " Decay",     0.0f, 1.0f, 0.6f);
        addFloat (prefix + "brightness", label + " Brightness",0.0f, 1.0f, 0.5f);
        addFloat (prefix + "stiffness",  label + " Stiffness", 0.0f, 0.95f,0.0f);
        addFloat (prefix + "morph",      label + " Morph",     0.0f, 1.0f, 0.0f);
        addFloat (prefix + "position",   label + " Position",  0.05f,0.95f,0.3f);
        addEnvTime(prefix + "adsr_a",    label + " Env Attack",  0.001f);
        addEnvTime(prefix + "adsr_d",    label + " Env Decay",   0.1f);
        addFloat  (prefix + "adsr_s",    label + " Env Sustain", 0.0f, 1.0f, 0.0f);
        addEnvTime(prefix + "adsr_r",    label + " Env Release", 1.0f);
    };

    for (int i = 0; i < 8; ++i)
        addKSOsc(i, indexedPrefix("ks", i));

    // ── Engine Configuration ──────────────────────────────────────────────────
    // Two engine pages (A=left, B=right), each covers 4 slot units.
    // Mode is treated as a pair: both pages at 1 = dual FM; otherwise both pages are Multi.
    addInt  ("engine_block_a_mode",   "Block A Mode",   0, 1, 0); // default Multi
    addInt  ("engine_block_b_mode",   "Block B Mode",   0, 1, 0); // default Multi
    // Slot types for Multi mode: 0=None 1=3D 2=VA 3=WT 4=KS 5=Granular 6=Terrain
    addInt  ("engine_block_a_slot_0", "Block A Slot 0", 0, 6, 1); // 3D
    addInt  ("engine_block_a_slot_1", "Block A Slot 1", 0, 6, 2); // VA
    addInt  ("engine_block_a_slot_2", "Block A Slot 2", 0, 6, 3); // WT
    addInt  ("engine_block_a_slot_3", "Block A Slot 3", 0, 6, 4); // KS
    addInt  ("engine_block_b_slot_0", "Block B Slot 0", 0, 6, 5); // Granular
    addInt  ("engine_block_b_slot_1", "Block B Slot 1", 0, 6, 6); // Terrain
    addInt  ("engine_block_b_slot_2", "Block B Slot 2", 0, 6, 0);
    addInt  ("engine_block_b_slot_3", "Block B Slot 3", 0, 6, 0);
    addFloat("engine_3d_detune",  "3D Ensemble Detune", 0.0f, 1.0f, 0.0f);
    addFloat("engine_fm_detune",  "FM Dual Detune",     0.0f, 100.0f, 0.0f);

    // ── Dual Filter ───────────────────────────────────────────────────────────
    juce::StringArray filterModes {
        "LP12","LP24","HP12","HP24","Bandpass","Notch",
        "Peak","LoShelf","HiShelf","Comb","AM+","AM-",
        "Ladder12","Ladder18","Ladder24","SEM Morph","MS Dual","Formant",
        "Juno-106","Juno-60","Jupiter-4","Jupiter-8","TB-303",
        "P5 Rev2 SSM","P5 Rev3 CEM","Prophet-600","Prophet-6","Pro-One"
    };
    juce::StringArray filterRouting { "Series","Parallel","A Only","B Only" };
    juce::StringArray filterOversampling { "Off", "2x", "4x" };

    for (auto slot : {"a", "b"})
    {
        juce::String s = juce::String("filter_") + slot + "_";
        addChoice(s + "type",      "Filter " + juce::String(slot).toUpperCase() + " Type", filterModes, 0);
        addFloat (s + "cutoff",    "Filter " + juce::String(slot).toUpperCase() + " Cutoff",
                  20.0f, 20000.0f, 20000.0f, 0.25f);
        addFloat (s + "resonance", "Filter " + juce::String(slot).toUpperCase() + " Resonance", 0.0f, 1.0f, 0.0f);
        addFloat (s + "drive",     "Filter " + juce::String(slot).toUpperCase() + " Drive",     0.0f, 1.0f, 0.0f);
        addFloat (s + "morph",     "Filter " + juce::String(slot).toUpperCase() + " Morph",     0.0f, 1.0f, 0.5f);
        addFloat (s + "saturation","Filter " + juce::String(slot).toUpperCase() + " Saturation",0.0f, 1.0f, 0.35f);
        addFloat (s + "bass_comp", "Filter " + juce::String(slot).toUpperCase() + " Bass Comp", 0.0f, 1.0f, 0.0f);
        addFloat (s + "gain",      "Filter " + juce::String(slot).toUpperCase() + " Output Gain",-24.0f, 24.0f, 0.0f);
        addFloat (s + "mix",       "Filter " + juce::String(slot).toUpperCase() + " Mix",       0.0f, 1.0f, 1.0f);
        addChoice(s + "oversample","Filter " + juce::String(slot).toUpperCase() + " Oversample", filterOversampling, 0);
        addFloat (s + "env_amt",   "Filter " + juce::String(slot).toUpperCase() + " Env Amt",  -1.0f, 1.0f, 0.0f);
        addFloat (s + "lfo_amt",   "Filter " + juce::String(slot).toUpperCase() + " LFO Amt",  -1.0f, 1.0f, 0.0f);
        addEnvTime(s + "adsr_a",    "Filter " + juce::String(slot).toUpperCase() + " Env A", 0.05f);
        addEnvTime(s + "adsr_d",    "Filter " + juce::String(slot).toUpperCase() + " Env D", 0.3f);
        addFloat (s + "adsr_s",    "Filter " + juce::String(slot).toUpperCase() + " Env S",  0.0f,   1.0f,  0.0f);
        addEnvTime(s + "adsr_r",    "Filter " + juce::String(slot).toUpperCase() + " Env R", 0.2f);
    }
    addChoice("filter_routing", "Filter Routing", filterRouting, 0);

    // ── LFOs 1-4 ─────────────────────────────────────────────────────────────
    juce::StringArray lfoShapes { "Sine","Triangle","Saw","RevSaw","Square","S&H","Smooth S&H" };

    for (int i = 1; i <= 4; ++i)
    {
        juce::String p = "lfo" + juce::String(i) + "_";
        addChoice(p + "shape",  "LFO" + juce::String(i) + " Shape",  lfoShapes, 0);
        addFloat (p + "rate",   "LFO" + juce::String(i) + " Rate",   0.01f, 100.0f, 1.0f, 0.3f);
        addBool  (p + "sync",   "LFO" + juce::String(i) + " Sync",   false);
        addFloat (p + "sync_rate", "LFO" + juce::String(i) + " Sync Rate", 0.0f, 10.0f, 5.0f);
        addFloat (p + "amount", "LFO" + juce::String(i) + " Amount", 0.0f, 1.0f, 0.5f);
        addFloat (p + "phase",  "LFO" + juce::String(i) + " Phase",  0.0f, 1.0f, 0.0f);
        addBool  (p + "retrig", "LFO" + juce::String(i) + " Retrig", false);
    }

    // ── Modulation Matrix (32 slots) ──────────────────────────────────────────
    juce::StringArray modSources = ModNames::sourceParameterNames();
    juce::StringArray modDests = ModNames::destParameterNames();

    for (int i = 0; i < 32; ++i)
    {
        juce::String p = "mod" + juce::String(i) + "_";
        addChoice(p + "source",  "Mod" + juce::String(i) + " Source",  modSources, 0);
        addChoice(p + "dest",    "Mod" + juce::String(i) + " Dest",    modDests, 0);
        addFloat (p + "amount",  "Mod" + juce::String(i) + " Amount", -1.0f, 1.0f, 0.0f);
        addBool  (p + "bipolar", "Mod" + juce::String(i) + " Bipolar", true);
    }

    // Macros
    for (int i = 1; i <= 8; ++i)
        addFloat("macro" + juce::String(i), "Macro " + juce::String(i), 0.0f, 1.0f, 0.0f);

    // ── Four-lane modulation step sequencer ──────────────────────────────────
    addFloat(StepSequencerMod::kDepthParamID, "Alien Sequencer", 0.0f, 1.0f, 0.45f);
    addChoice(StepSequencerMod::kDivisionParamID, "Mod Seq Division",
              { "1/4", "1/8", "1/16", "1/32", "1/64", "1/8T", "1/16T", "1/8D", "1/16D" }, 2);
    addChoice(StepSequencerMod::kSubdivisionParamID, "Granular Subdivision",
              { "x1", "x2", "x4", "x8" }, 2);
    addChoice(StepSequencerMod::kLengthParamID, "Mod Seq Length",
              { "8", "16", "24", "32" }, 1);

    const int defaultTargets[] = {
        (int) ModDest::FilterA_Cutoff,
        (int) ModDest::FilterA_Resonance,
        (int) ModDest::WT_Position,
        (int) ModDest::Gran_Freeze
    };

    for (int lane = 0; lane < StepSequencerMod::kNumLanes; ++lane)
    {
        addChoice(StepSequencerMod::laneTargetParamID(lane),
                  "SEQ " + juce::String(lane + 1) + " Target",
                  modDests,
                  defaultTargets[lane]);

        for (int step = 0; step < StepSequencerMod::kNumSteps; ++step)
        {
            float value = 0.0f;
            if (lane == 0)
                value = 0.5f + 0.5f * std::sin(((float) step / (float) StepSequencerMod::kNumSteps)
                                                * juce::MathConstants<float>::twoPi);
            else if (lane == 1)
                value = step % 4 == 0 ? 0.9f : 0.18f;
            else if (lane == 2)
                value = (float) ((step * 5 + 3) % StepSequencerMod::kNumSteps)
                      / (float) (StepSequencerMod::kNumSteps - 1);
            else
                value = step % 5 == 0 ? 1.0f : 0.1f;

            addFloat(StepSequencerMod::stepParamID(lane, step),
                     "SEQ " + juce::String(lane + 1) + " Step " + juce::String(step + 1),
                     0.0f, 1.0f, value);
        }
    }

    // ── Arpeggiator ───────────────────────────────────────────────────────────
    juce::StringArray arpPatterns { "Up","Down","Up-Down","Down-Up","As Played","Random","Random Latch" };
    juce::StringArray arpRates    { "1/32","1/16","1/8","3/16","1/4","3/8","1/2","3/4","1 Bar" };

    addBool  ("arp_on",      "Arp On",       false);
    addChoice("arp_pattern", "Arp Pattern",  arpPatterns, 0);
    addChoice("arp_rate",    "Arp Rate",     arpRates, 3);
    addBool  ("arp_sync",    "Arp Sync",     true);
    addInt   ("arp_octaves", "Arp Octaves",  1, 4, 1);
    addFloat ("arp_gate",    "Arp Gate",     0.01f, 1.0f, 0.8f);
    addFloat ("arp_swing",   "Arp Swing",    0.0f, 0.66f, 0.0f);
    addBool  ("arp_latch",   "Arp Latch",    false);

    return { params.begin(), params.end() };
}

//==============================================================================
GLIESE710AudioProcessor::GLIESE710AudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "State", buildLayout()),
      presetManager(apvts)
{
}

GLIESE710AudioProcessor::~GLIESE710AudioProcessor() = default;

bool GLIESE710AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void GLIESE710AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synthEngine.prepare({ sampleRate, (juce::uint32)samplesPerBlock, 2 });
    synthEngine.setAPVTS(&apvts);
}

void GLIESE710AudioProcessor::releaseResources()
{
    synthEngine.reset();
}

void GLIESE710AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    std::optional<double> ppqPosition;
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpm = position->getBpm())
                synthEngine.setHostBPM(*bpm);
            if (auto ppq = position->getPpqPosition())
                ppqPosition = *ppq;
        }
    }

    synthEngine.setHostPosition(ppqPosition);

    synthEngine.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

bool GLIESE710AudioProcessor::importTerrainHeightmap(int terrainIndex, const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    auto image = juce::ImageFileFormat::loadFrom(file);
    if (!image.isValid())
        return false;

    suspendProcessing(true);
    const bool ok = synthEngine.setTerrainHeightmap(terrainIndex, image);
    suspendProcessing(false);
    return ok;
}

//==============================================================================
juce::AudioProcessorEditor* GLIESE710AudioProcessor::createEditor()
{
    return new GLIESE710AudioProcessorEditor(*this);
}

void GLIESE710AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void GLIESE710AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid())
            apvts.replaceState(tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GLIESE710AudioProcessor();
}
