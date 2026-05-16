#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    // ── 3D Oscillator ─────────────────────────────────────────────────────────
    addInt  ("osc_3d_field",    "3D Field",    0, 17, 0);
    addInt  ("osc_3d_path",     "3D Path",     0, 4, 0);
    addFloat("osc_3d_radius",   "3D Radius",   0.1f, 4.0f, 1.0f);
    addFloat("osc_3d_depth",    "3D Depth",    0.0f, 4.0f, 1.0f);
    addFloat("osc_3d_warp",     "3D Warp",     0.0f, 2.0f, 0.25f);
    addFloat("osc_3d_rotation", "3D Rotation", 0.0f, 1.0f, 0.0f);
    addFloat("osc_3d_chaos",    "3D Chaos",    0.0f, 1.0f, 0.0f);
    addFloat("osc_3d_morph",    "3D Morph",    0.0f, 1.0f, 0.0f);
    addFloat("osc_3d_field_freq", "3D Field Freq", 0.25f, 16.0f, 1.0f, 0.4f);
    addFloat("osc_3d_z_ratio",    "3D Z Ratio",    0.25f, 4.0f,  1.0f, 0.7f);
    addFloat("osc_3d_fold",       "3D Fold",       0.0f,  1.0f,  0.0f);
    addFloat("osc_3d_drive",      "3D Drive",      0.25f, 4.0f,  1.0f, 0.5f);
    addBool ("osc_3d_on",       "3D Enable",   true);
    addFloat("osc_3d_level",    "3D Level",    0.0f, 1.0f, 0.8f);
    addFloat("osc_3d_octave",   "3D Octave",   -3.0f, 3.0f, 0.0f);
    addEnvTime("osc_3d_adsr_a",   "3D Env Attack",  0.01f);
    addEnvTime("osc_3d_adsr_d",   "3D Env Decay",   0.2f);
    addFloat("osc_3d_adsr_s",   "3D Env Sustain", 0.0f,   1.0f,  1.0f);
    addEnvTime("osc_3d_adsr_r",   "3D Env Release", 0.3f);

    // ── FM Engine ─────────────────────────────────────────────────────────────
    addBool ("osc_fm_on",        "FM Enable",    true);
    addInt  ("osc_fm_algorithm", "FM Algorithm", 0, 39, 0);
    addFloat("osc_fm_level",     "FM Level",     0.0f, 1.0f, 0.0f);
    addFloat("osc_fm_octave",    "FM Octave",   -3.0f, 3.0f, 0.0f);
    addEnvTime("osc_fm_adsr_a",    "FM Env Attack",  0.01f);
    addEnvTime("osc_fm_adsr_d",    "FM Env Decay",   0.2f);
    addFloat("osc_fm_adsr_s",    "FM Env Sustain", 0.0f,   1.0f,  1.0f);
    addEnvTime("osc_fm_adsr_r",    "FM Env Release", 0.3f);

    juce::StringArray opWaveforms {
        "Sine","Triangle","Saw","Square","Pulse","Noise",
        "All Wide","All Narrow","Odd Wide","Odd Narrow","Res Wide","Res Narrow"
    };

    for (int i = 0; i < 10; ++i)
    {
        juce::String prefix = "op" + juce::String(i) + "_";
        addInt   (prefix + "coarse",   "Op" + juce::String(i) + " Coarse",   0,  31, i == 0 ? 1 : 1);
        addFloat (prefix + "fine",     "Op" + juce::String(i) + " Fine",    -1.0f, 1.0f, 0.0f);
        addFloat (prefix + "detune",   "Op" + juce::String(i) + " Detune", -15.0f, 15.0f, 0.0f);
        addFloat (prefix + "level",    "Op" + juce::String(i) + " Level",    0.0f, 1.0f, i == 0 ? 1.0f : 0.5f);
        addFloat (prefix + "feedback", "Op" + juce::String(i) + " Feedback", 0.0f, 1.0f, 0.0f);
        addBool  (prefix + "fixed",    "Op" + juce::String(i) + " Fixed Freq", false);
        addChoice(prefix + "wave",     "Op" + juce::String(i) + " Wave",     opWaveforms, 0);
        addFloat (prefix + "skirt",    "Op" + juce::String(i) + " Spectral Skirt", 0.0f, 1.0f, 0.35f);
        addFloat (prefix + "resonance","Op" + juce::String(i) + " Spectral Resonance", 0.0f, 1.0f, 0.25f);
        addBool  (prefix + "key_reset","Op" + juce::String(i) + " Key On Reset", true);
        addFloat (prefix + "vel",      "Op" + juce::String(i) + " Vel Depth",   0.0f, 1.0f, 0.0f);
        addFloat (prefix + "key",      "Op" + juce::String(i) + " Key Scale",  -1.0f, 1.0f, 0.0f);
        addEnvTime(prefix + "adsr_a",   "Op" + juce::String(i) + " Attack",  0.01f);
        addEnvTime(prefix + "adsr_d",   "Op" + juce::String(i) + " Decay",   0.5f);
        addFloat (prefix + "adsr_s",   "Op" + juce::String(i) + " Sustain", 0.0f,   1.0f,  0.8f);
        addEnvTime(prefix + "adsr_r",   "Op" + juce::String(i) + " Release", 0.3f);
    }

    // ── Wavetable Oscillator ──────────────────────────────────────────────────
    addBool ("osc_wt_on",    "WT Enable",   true);
    addChoice("osc_wt_table", "WT Table",
              {"Sine","Triangle","Saw","Square","Pulse","Half Sine","Abs Sine","Sine Cube",
               "Hollow Odd","Even Glass","Vocal A","Metallic","Sync Saw","Reso Fold","Noisy Digital"}, 0);
    addChoice("osc_wt_warp_mode", "WT Warp Mode",
              {"Off","Bend+","Bend-","Bend +/-","Mirror","Sync","Window Sync",
               "Quantize","Fold","Formant","PD Saw","PD Square","PD Pulse",
               "PD Resonant","PD Formant"}, 0);
    addFloat("osc_wt_pos",        "WT Position",       0.0f, 1.0f, 0.0f);
    addFloat("osc_wt_warp",       "WT Warp Amount",    0.0f, 1.0f, 0.0f);
    addFloat("osc_wt_pd_break",   "WT PD Breakpoint",  0.0f, 1.0f, 0.75f);
    addInt  ("osc_wt_unison",     "WT Unison",         1, 8, 1);
    addFloat("osc_wt_detune",     "WT Detune",         0.0f, 1.0f, 0.0f);
    addFloat("osc_wt_spread",     "WT Stereo Spread",  0.0f, 1.0f, 0.0f);
    addFloat("osc_wt_pos_spread", "WT Position Spread",0.0f, 1.0f, 0.0f);
    addFloat("osc_wt_phase",      "WT Phase Offset",   0.0f, 1.0f, 0.0f);
    addFloat("osc_wt_random",     "WT Random Phase",   0.0f, 1.0f, 0.0f);
    addBool ("osc_wt_retrig",     "WT Retrigger",      true);
    addFloat("osc_wt_level",      "WT Level",          0.0f, 1.0f, 0.0f);
    addFloat("osc_wt_octave",     "WT Octave",        -3.0f, 3.0f, 0.0f);
    addEnvTime("osc_wt_adsr_a", "WT Env Attack",  0.01f);
    addEnvTime("osc_wt_adsr_d", "WT Env Decay",   0.2f);
    addFloat("osc_wt_adsr_s", "WT Env Sustain", 0.0f,   1.0f,  1.0f);
    addEnvTime("osc_wt_adsr_r", "WT Env Release", 0.3f);

    // ── Granular Oscillator ──────────────────────────────────────────────────
    addBool ("osc_grain_on",    "Granular Enable", true);
    addChoice("osc_grain_trigger_mode", "Granular Trigger Mode",
              {"Free Cloud","Burst","Audio Rate"}, 0);
    addChoice("osc_grain_window", "Granular Window",
              {"Hann","Triangle","Gaussian","Tukey","Rect","Exp Attack","Exp Decay"}, 0);
    addFloat("osc_grain_position", "Grain Position", 0.0f, 1.0f, 0.5f);
    addFloat("osc_grain_size",     "Grain Size",     5.0f, 1000.0f, 120.0f, 0.35f);
    addFloat("osc_grain_density",  "Grain Density",  0.1f, 1200.0f, 35.0f, 0.32f);
    addFloat("osc_grain_pitch",    "Grain Pitch",   -48.0f, 48.0f, 0.0f);
    addFloat("osc_grain_spray",    "Grain Spray",    0.0f, 1.0f, 0.18f);
    addFloat("osc_grain_jitter",   "Grain Jitter",   0.0f, 1.0f, 0.04f);
    addFloat("osc_grain_pan",      "Grain Pan Spread", 0.0f, 1.0f, 0.7f);
    addFloat("osc_grain_reverse",  "Grain Reverse",  0.0f, 1.0f, 0.0f);
    addFloat("osc_grain_scan",     "Grain Scan",    -1.0f, 1.0f, 0.0f);
    addFloat("osc_grain_freeze",   "Grain Freeze",   0.0f, 1.0f, 0.0f);
    addChoice("osc_grain_body_type", "Grain Body Type",
              {"Off","Glass","Metal","Wood","Plate","String","Drum","Pipe","Alien"}, 0);
    addFloat("osc_grain_body_root",   "Grain Body Root",   -48.0f, 48.0f, 0.0f);
    addFloat("osc_grain_body_size",   "Grain Body Size",    0.0f, 1.0f, 0.5f);
    addFloat("osc_grain_body_decay",  "Grain Body Decay",   0.0f, 1.0f, 0.5f);
    addFloat("osc_grain_body_damp",   "Grain Body Damping", 0.0f, 1.0f, 0.35f);
    addFloat("osc_grain_body_bright", "Grain Body Bright",  0.0f, 1.0f, 0.6f);
    addFloat("osc_grain_body_inharm", "Grain Body Inharm",  0.0f, 1.0f, 0.2f);
    addFloat("osc_grain_body_excite", "Grain Body Excite",  0.0f, 1.0f, 0.7f);
    addFloat("osc_grain_body_mix",    "Grain Body Mix",     0.0f, 1.0f, 0.0f);
    addFloat("osc_grain_body_spread", "Grain Body Spread",  0.0f, 1.0f, 0.5f);
    addInt  ("osc_grain_body_modes",  "Grain Body Modes",   4, 32, 16);
    addFloat("osc_grain_level",    "Granular Level", 0.0f, 1.0f, 0.0f);
    addFloat("osc_grain_octave",   "Granular Octave",-3.0f, 3.0f, 0.0f);
    addEnvTime("osc_grain_adsr_a", "Granular Env Attack",  0.01f);
    addEnvTime("osc_grain_adsr_d", "Granular Env Decay",   0.2f);
    addFloat("osc_grain_adsr_s",   "Granular Env Sustain", 0.0f, 1.0f, 1.0f);
    addEnvTime("osc_grain_adsr_r", "Granular Env Release", 0.4f);

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
    addVAOsc(2, "osc_va2_", false);
    addVAOsc(3, "osc_va3_", false);
    addVAOsc(4, "osc_va4_", false);
    addVAOsc(5, "osc_va5_", false);
    addVAOsc(6, "osc_va6_", false);
    addVAOsc(7, "osc_va7_", false);
    addVAOsc(8, "osc_va8_", false);

    // ── Karplus-Strong Oscillator ─────────────────────────────────────────────
    addBool  ("osc_ks_on",         "KS Enable",    true);
    addFloat ("osc_ks_level",      "KS Level",     0.0f, 1.0f, 0.0f);
    addFloat ("osc_ks_octave",     "KS Octave",   -3.0f, 3.0f, 0.0f);
    addFloat ("osc_ks_decay",      "KS Decay",     0.0f, 1.0f, 0.6f);
    addFloat ("osc_ks_brightness", "KS Brightness",0.0f, 1.0f, 0.5f);
    addFloat ("osc_ks_stiffness",  "KS Stiffness", 0.0f, 0.95f,0.0f);
    addFloat ("osc_ks_morph",      "KS Morph",     0.0f, 1.0f, 0.0f);
    addFloat ("osc_ks_position",   "KS Position",  0.05f,0.95f,0.3f);
    addEnvTime("osc_ks_adsr_a",    "KS Env Attack",  0.001f);
    addEnvTime("osc_ks_adsr_d",    "KS Env Decay",   0.1f);
    addFloat  ("osc_ks_adsr_s",    "KS Env Sustain", 0.0f, 1.0f, 0.0f);
    addEnvTime("osc_ks_adsr_r",    "KS Env Release", 1.0f);

    // ── Engine Configuration ──────────────────────────────────────────────────
    // Two engine pages (A=left, B=right), each covers 4 slot units.
    // Mode is treated as a pair: both pages at 1 = dual FM; otherwise both pages are Multi.
    addInt  ("engine_block_a_mode",   "Block A Mode",   0, 1, 0); // default Multi
    addInt  ("engine_block_b_mode",   "Block B Mode",   0, 1, 0); // default Multi
    // Slot types for Multi mode: 0=None 1=3D 2=VA 3=WT 4=KS 5=Granular
    addInt  ("engine_block_a_slot_0", "Block A Slot 0", 0, 5, 1); // 3D
    addInt  ("engine_block_a_slot_1", "Block A Slot 1", 0, 5, 2); // VA
    addInt  ("engine_block_a_slot_2", "Block A Slot 2", 0, 5, 3); // WT
    addInt  ("engine_block_a_slot_3", "Block A Slot 3", 0, 5, 4); // KS
    addInt  ("engine_block_b_slot_0", "Block B Slot 0", 0, 5, 0);
    addInt  ("engine_block_b_slot_1", "Block B Slot 1", 0, 5, 0);
    addInt  ("engine_block_b_slot_2", "Block B Slot 2", 0, 5, 0);
    addInt  ("engine_block_b_slot_3", "Block B Slot 3", 0, 5, 0);
    addFloat("engine_3d_detune",  "3D Ensemble Detune", 0.0f, 1.0f, 0.0f);
    addFloat("engine_fm_detune",  "FM Dual Detune",     0.0f, 100.0f, 0.0f);

    // ── Dual Filter ───────────────────────────────────────────────────────────
    juce::StringArray filterModes {
        "LP12","LP24","HP12","HP24","Bandpass","Notch",
        "Peak","LoShelf","HiShelf","Comb","AM+","AM-",
        "Ladder12","Ladder18","Ladder24","SEM Morph","MS Dual","Formant"
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
    juce::StringArray modSources {
        "None",
        "LFO1","LFO2","LFO3","LFO4",
        "Env_Master","Env_3D","Env_FM","Env_WT","Env_VA","Env_FilterA","Env_FilterB",
        "Velocity","Aftertouch","ModWheel","PitchBend","CC74","ChanPressure",
        "Macro1","Macro2","Macro3","Macro4","Macro5","Macro6","Macro7","Macro8"
    };

    juce::StringArray modDests {
        "None",
        "OscPitch_All",
        "Osc3D_Radius","Osc3D_Depth","Osc3D_Warp","Osc3D_Chaos","Osc3D_Morph","Osc3D_Level",
        "FM_Op0_Level","FM_Op1_Level","FM_Op2_Level","FM_Op3_Level","FM_Op4_Level",
        "FM_Op5_Level","FM_Op6_Level","FM_Op7_Level","FM_Op8_Level","FM_Op9_Level",
        "FM_Op0_Ratio","FM_Op1_Ratio","FM_Op2_Ratio","FM_Op3_Ratio","FM_Op4_Ratio",
        "WT_Position","WT_Level",
        "VA_PW","VA_Detune","VA_Spread","VA_Level",
        "FilterA_Cutoff","FilterA_Resonance","FilterA_Drive",
        "FilterB_Cutoff","FilterB_Resonance","FilterB_Drive",
        "LFO1_Rate","LFO2_Rate","LFO3_Rate","LFO4_Rate",
        "LFO1_Amount","LFO2_Amount","LFO3_Amount","LFO4_Amount",
        "Master_Volume","Master_Pan",
        "Arp_Gate","Arp_Rate",
        "Osc3D_FieldFreq","Osc3D_ZRatio","Osc3D_Fold","Osc3D_Drive",
        "VA_Shape","VA_Ring","VA_Fold",
        "VA2_Shape","VA2_PW","VA2_Detune","VA2_Spread","VA2_Level","VA2_Ring","VA2_Fold",
        "VA3_Shape","VA3_PW","VA3_Detune","VA3_Spread","VA3_Level","VA3_Ring","VA3_Fold",
        "VA4_Shape","VA4_PW","VA4_Detune","VA4_Spread","VA4_Level","VA4_Ring","VA4_Fold",
        "LFO1_SyncRate","LFO2_SyncRate","LFO3_SyncRate","LFO4_SyncRate",
        "WT_Warp","WT_PD_Break",
        "Gran_Position","Gran_Size","Gran_Density","Gran_Pitch","Gran_Spray","Gran_Freeze","Gran_Level",
        "Gran_BodyMix","Gran_BodySize","Gran_BodyDamping","Gran_BodyInharm"
    };

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

    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm())
                synthEngine.setHostBPM(*bpm);

    synthEngine.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
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
