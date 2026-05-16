#pragma once

#include "ModDest.h"
#include "ModSource.h"
#include <JuceHeader.h>
#include <array>

namespace ModNames
{
inline constexpr std::array<const char*, (int) ModSource::NumSources> kSourceDisplayNames {
    "None",
    "LFO1", "LFO2", "LFO3", "LFO4",
    "Env Master", "Env 3D", "Env FM", "Env WT", "Env VA", "Env Filt A", "Env Filt B",
    "Velocity", "Aftertouch", "Mod Wheel", "Pitch Bend", "CC74", "Chan Press",
    "Macro1", "Macro2", "Macro3", "Macro4", "Macro5", "Macro6", "Macro7", "Macro8"
};

inline constexpr std::array<const char*, (int) ModSource::NumSources> kSourceParameterNames {
    "None",
    "LFO1", "LFO2", "LFO3", "LFO4",
    "Env_Master", "Env_3D", "Env_FM", "Env_WT", "Env_VA", "Env_FilterA", "Env_FilterB",
    "Velocity", "Aftertouch", "ModWheel", "PitchBend", "CC74", "ChanPressure",
    "Macro1", "Macro2", "Macro3", "Macro4", "Macro5", "Macro6", "Macro7", "Macro8"
};

inline constexpr std::array<const char*, (int) ModDest::NumDests> kDestDisplayNames {
    "None",
    "Pitch All",
    "3D Radius", "3D Depth", "3D Warp", "3D Chaos", "3D Morph", "3D Level",
    "FM Op0 Lvl", "FM Op1 Lvl", "FM Op2 Lvl", "FM Op3 Lvl", "FM Op4 Lvl",
    "FM Op5 Lvl", "FM Op6 Lvl", "FM Op7 Lvl", "FM Op8 Lvl", "FM Op9 Lvl",
    "FM Op0 Rat", "FM Op1 Rat", "FM Op2 Rat", "FM Op3 Rat", "FM Op4 Rat",
    "WT Position", "WT Level",
    "VA PW", "VA Detune", "VA Spread", "VA Level",
    "Filt A Cut", "Filt A Res", "Filt A Drv",
    "Filt B Cut", "Filt B Res", "Filt B Drv",
    "LFO1 Rate", "LFO2 Rate", "LFO3 Rate", "LFO4 Rate",
    "LFO1 Amt", "LFO2 Amt", "LFO3 Amt", "LFO4 Amt",
    "Master Vol", "Master Pan",
    "Arp Gate", "Arp Rate",
    "3D FieldFreq", "3D Z Ratio", "3D Fold", "3D Drive",
    "VA Shape", "VA Ring", "VA Fold",
    "VA2 Shape", "VA2 PW", "VA2 Detune", "VA2 Spread", "VA2 Level", "VA2 Ring", "VA2 Fold",
    "VA3 Shape", "VA3 PW", "VA3 Detune", "VA3 Spread", "VA3 Level", "VA3 Ring", "VA3 Fold",
    "VA4 Shape", "VA4 PW", "VA4 Detune", "VA4 Spread", "VA4 Level", "VA4 Ring", "VA4 Fold",
    "LFO1 SyncRt", "LFO2 SyncRt", "LFO3 SyncRt", "LFO4 SyncRt",
    "WT Warp", "WT PD Brk",
    "Gran Pos", "Gran Size", "Gran Density", "Gran Pitch", "Gran Spray", "Gran Freeze", "Gran Level",
    "Gran BodyMix", "Gran BodySize", "Gran Damping", "Gran Inharm"
};

inline constexpr std::array<const char*, (int) ModDest::NumDests> kDestParameterNames {
    "None",
    "OscPitch_All",
    "Osc3D_Radius", "Osc3D_Depth", "Osc3D_Warp", "Osc3D_Chaos", "Osc3D_Morph", "Osc3D_Level",
    "FM_Op0_Level", "FM_Op1_Level", "FM_Op2_Level", "FM_Op3_Level", "FM_Op4_Level",
    "FM_Op5_Level", "FM_Op6_Level", "FM_Op7_Level", "FM_Op8_Level", "FM_Op9_Level",
    "FM_Op0_Ratio", "FM_Op1_Ratio", "FM_Op2_Ratio", "FM_Op3_Ratio", "FM_Op4_Ratio",
    "WT_Position", "WT_Level",
    "VA_PW", "VA_Detune", "VA_Spread", "VA_Level",
    "FilterA_Cutoff", "FilterA_Resonance", "FilterA_Drive",
    "FilterB_Cutoff", "FilterB_Resonance", "FilterB_Drive",
    "LFO1_Rate", "LFO2_Rate", "LFO3_Rate", "LFO4_Rate",
    "LFO1_Amount", "LFO2_Amount", "LFO3_Amount", "LFO4_Amount",
    "Master_Volume", "Master_Pan",
    "Arp_Gate", "Arp_Rate",
    "Osc3D_FieldFreq", "Osc3D_ZRatio", "Osc3D_Fold", "Osc3D_Drive",
    "VA_Shape", "VA_Ring", "VA_Fold",
    "VA2_Shape", "VA2_PW", "VA2_Detune", "VA2_Spread", "VA2_Level", "VA2_Ring", "VA2_Fold",
    "VA3_Shape", "VA3_PW", "VA3_Detune", "VA3_Spread", "VA3_Level", "VA3_Ring", "VA3_Fold",
    "VA4_Shape", "VA4_PW", "VA4_Detune", "VA4_Spread", "VA4_Level", "VA4_Ring", "VA4_Fold",
    "LFO1_SyncRate", "LFO2_SyncRate", "LFO3_SyncRate", "LFO4_SyncRate",
    "WT_Warp", "WT_PD_Break",
    "Gran_Position", "Gran_Size", "Gran_Density", "Gran_Pitch", "Gran_Spray", "Gran_Freeze", "Gran_Level",
    "Gran_BodyMix", "Gran_BodySize", "Gran_BodyDamping", "Gran_BodyInharm"
};

inline juce::StringArray toStringArray(const auto& names)
{
    juce::StringArray result;
    for (auto* name : names)
        result.add(name);
    return result;
}

inline juce::StringArray sourceDisplayNames()   { return toStringArray(kSourceDisplayNames); }
inline juce::StringArray sourceParameterNames() { return toStringArray(kSourceParameterNames); }
inline juce::StringArray destDisplayNames()     { return toStringArray(kDestDisplayNames); }
inline juce::StringArray destParameterNames()   { return toStringArray(kDestParameterNames); }
}
