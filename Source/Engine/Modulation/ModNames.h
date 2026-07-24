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
    "FM Op5 Rat", "FM Op6 Rat", "FM Op7 Rat", "FM Op8 Rat", "FM Op9 Rat",
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
    "Gran BodyMix", "Gran BodySize", "Gran Damping", "Gran Inharm",
    "3D Filt Cut", "3D Filt Res", "3D Filt Drv", "3D Filt Morph", "3D Filt Mix",
    "FM Filt Cut", "FM Filt Res", "FM Filt Drv", "FM Filt Morph", "FM Filt Mix",
    "WT Filt Cut", "WT Filt Res", "WT Filt Drv", "WT Filt Morph", "WT Filt Mix",
    "VA Filt Cut", "VA Filt Res", "VA Filt Drv", "VA Filt Morph", "VA Filt Mix",
    "KS Filt Cut", "KS Filt Res", "KS Filt Drv", "KS Filt Morph", "KS Filt Mix",
    "Gran Filt Cut", "Gran Filt Res", "Gran Filt Drv", "Gran Filt Morph", "Gran Filt Mix",
    "Terrain Filt Cut", "Terrain Filt Res", "Terrain Filt Drv", "Terrain Filt Morph", "Terrain Filt Mix",
    "VA5 Shape", "VA5 PW", "VA5 Detune", "VA5 Spread", "VA5 Level", "VA5 Ring", "VA5 Fold",
    "VA6 Shape", "VA6 PW", "VA6 Detune", "VA6 Spread", "VA6 Level", "VA6 Ring", "VA6 Fold",
    "VA7 Shape", "VA7 PW", "VA7 Detune", "VA7 Spread", "VA7 Level", "VA7 Ring", "VA7 Fold",
    "VA8 Shape", "VA8 PW", "VA8 Detune", "VA8 Spread", "VA8 Level", "VA8 Ring", "VA8 Fold",
    "Terrain Center X", "Terrain Center Y", "Terrain Radius X", "Terrain Radius Y",
    "Terrain Ratio X", "Terrain Ratio Y", "Terrain Smooth", "Terrain Morph", "Terrain Drive",
    "FM B Op0 Lvl", "FM B Op1 Lvl", "FM B Op2 Lvl", "FM B Op3 Lvl", "FM B Op4 Lvl",
    "FM B Op5 Lvl", "FM B Op6 Lvl", "FM B Op7 Lvl", "FM B Op8 Lvl", "FM B Op9 Lvl",
    "FM B Op0 Rat", "FM B Op1 Rat", "FM B Op2 Rat", "FM B Op3 Rat", "FM B Op4 Rat",
    "FM B Op5 Rat", "FM B Op6 Rat", "FM B Op7 Rat", "FM B Op8 Rat", "FM B Op9 Rat",
    "Terrain2 Center X", "Terrain2 Center Y", "Terrain2 Radius X", "Terrain2 Radius Y",
    "Terrain2 Ratio X", "Terrain2 Ratio Y", "Terrain2 Smooth", "Terrain2 Morph", "Terrain2 Drive",
    "Terrain3 Center X", "Terrain3 Center Y", "Terrain3 Radius X", "Terrain3 Radius Y",
    "Terrain3 Ratio X", "Terrain3 Ratio Y", "Terrain3 Smooth", "Terrain3 Morph", "Terrain3 Drive",
    "Terrain4 Center X", "Terrain4 Center Y", "Terrain4 Radius X", "Terrain4 Radius Y",
    "Terrain4 Ratio X", "Terrain4 Ratio Y", "Terrain4 Smooth", "Terrain4 Morph", "Terrain4 Drive",
    "Terrain5 Center X", "Terrain5 Center Y", "Terrain5 Radius X", "Terrain5 Radius Y",
    "Terrain5 Ratio X", "Terrain5 Ratio Y", "Terrain5 Smooth", "Terrain5 Morph", "Terrain5 Drive",
    "Terrain6 Center X", "Terrain6 Center Y", "Terrain6 Radius X", "Terrain6 Radius Y",
    "Terrain6 Ratio X", "Terrain6 Ratio Y", "Terrain6 Smooth", "Terrain6 Morph", "Terrain6 Drive",
    "Terrain7 Center X", "Terrain7 Center Y", "Terrain7 Radius X", "Terrain7 Radius Y",
    "Terrain7 Ratio X", "Terrain7 Ratio Y", "Terrain7 Smooth", "Terrain7 Morph", "Terrain7 Drive",
    "Terrain8 Center X", "Terrain8 Center Y", "Terrain8 Radius X", "Terrain8 Radius Y",
    "Terrain8 Ratio X", "Terrain8 Ratio Y", "Terrain8 Smooth", "Terrain8 Morph", "Terrain8 Drive"
};

inline constexpr std::array<const char*, (int) ModDest::NumDests> kDestParameterNames {
    "None",
    "OscPitch_All",
    "Osc3D_Radius", "Osc3D_Depth", "Osc3D_Warp", "Osc3D_Chaos", "Osc3D_Morph", "Osc3D_Level",
    "FM_Op0_Level", "FM_Op1_Level", "FM_Op2_Level", "FM_Op3_Level", "FM_Op4_Level",
    "FM_Op5_Level", "FM_Op6_Level", "FM_Op7_Level", "FM_Op8_Level", "FM_Op9_Level",
    "FM_Op0_Ratio", "FM_Op1_Ratio", "FM_Op2_Ratio", "FM_Op3_Ratio", "FM_Op4_Ratio",
    "FM_Op5_Ratio", "FM_Op6_Ratio", "FM_Op7_Ratio", "FM_Op8_Ratio", "FM_Op9_Ratio",
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
    "Gran_BodyMix", "Gran_BodySize", "Gran_BodyDamping", "Gran_BodyInharm",
    "EngFilt3D_Cutoff", "EngFilt3D_Resonance", "EngFilt3D_Drive", "EngFilt3D_Morph", "EngFilt3D_Mix",
    "EngFiltFM_Cutoff", "EngFiltFM_Resonance", "EngFiltFM_Drive", "EngFiltFM_Morph", "EngFiltFM_Mix",
    "EngFiltWT_Cutoff", "EngFiltWT_Resonance", "EngFiltWT_Drive", "EngFiltWT_Morph", "EngFiltWT_Mix",
    "EngFiltVA_Cutoff", "EngFiltVA_Resonance", "EngFiltVA_Drive", "EngFiltVA_Morph", "EngFiltVA_Mix",
    "EngFiltKS_Cutoff", "EngFiltKS_Resonance", "EngFiltKS_Drive", "EngFiltKS_Morph", "EngFiltKS_Mix",
    "EngFiltGran_Cutoff", "EngFiltGran_Resonance", "EngFiltGran_Drive", "EngFiltGran_Morph", "EngFiltGran_Mix",
    "EngFiltTerrain_Cutoff", "EngFiltTerrain_Resonance", "EngFiltTerrain_Drive", "EngFiltTerrain_Morph", "EngFiltTerrain_Mix",
    "VA5_Shape", "VA5_PW", "VA5_Detune", "VA5_Spread", "VA5_Level", "VA5_Ring", "VA5_Fold",
    "VA6_Shape", "VA6_PW", "VA6_Detune", "VA6_Spread", "VA6_Level", "VA6_Ring", "VA6_Fold",
    "VA7_Shape", "VA7_PW", "VA7_Detune", "VA7_Spread", "VA7_Level", "VA7_Ring", "VA7_Fold",
    "VA8_Shape", "VA8_PW", "VA8_Detune", "VA8_Spread", "VA8_Level", "VA8_Ring", "VA8_Fold",
    "Terrain_CenterX", "Terrain_CenterY", "Terrain_RadiusX", "Terrain_RadiusY",
    "Terrain_RatioX", "Terrain_RatioY", "Terrain_Smoothing", "Terrain_Morph", "Terrain_Drive",
    "FM_B_Op0_Level", "FM_B_Op1_Level", "FM_B_Op2_Level", "FM_B_Op3_Level", "FM_B_Op4_Level",
    "FM_B_Op5_Level", "FM_B_Op6_Level", "FM_B_Op7_Level", "FM_B_Op8_Level", "FM_B_Op9_Level",
    "FM_B_Op0_Ratio", "FM_B_Op1_Ratio", "FM_B_Op2_Ratio", "FM_B_Op3_Ratio", "FM_B_Op4_Ratio",
    "FM_B_Op5_Ratio", "FM_B_Op6_Ratio", "FM_B_Op7_Ratio", "FM_B_Op8_Ratio", "FM_B_Op9_Ratio",
    "Terrain2_CenterX", "Terrain2_CenterY", "Terrain2_RadiusX", "Terrain2_RadiusY",
    "Terrain2_RatioX", "Terrain2_RatioY", "Terrain2_Smoothing", "Terrain2_Morph", "Terrain2_Drive",
    "Terrain3_CenterX", "Terrain3_CenterY", "Terrain3_RadiusX", "Terrain3_RadiusY",
    "Terrain3_RatioX", "Terrain3_RatioY", "Terrain3_Smoothing", "Terrain3_Morph", "Terrain3_Drive",
    "Terrain4_CenterX", "Terrain4_CenterY", "Terrain4_RadiusX", "Terrain4_RadiusY",
    "Terrain4_RatioX", "Terrain4_RatioY", "Terrain4_Smoothing", "Terrain4_Morph", "Terrain4_Drive",
    "Terrain5_CenterX", "Terrain5_CenterY", "Terrain5_RadiusX", "Terrain5_RadiusY",
    "Terrain5_RatioX", "Terrain5_RatioY", "Terrain5_Smoothing", "Terrain5_Morph", "Terrain5_Drive",
    "Terrain6_CenterX", "Terrain6_CenterY", "Terrain6_RadiusX", "Terrain6_RadiusY",
    "Terrain6_RatioX", "Terrain6_RatioY", "Terrain6_Smoothing", "Terrain6_Morph", "Terrain6_Drive",
    "Terrain7_CenterX", "Terrain7_CenterY", "Terrain7_RadiusX", "Terrain7_RadiusY",
    "Terrain7_RatioX", "Terrain7_RatioY", "Terrain7_Smoothing", "Terrain7_Morph", "Terrain7_Drive",
    "Terrain8_CenterX", "Terrain8_CenterY", "Terrain8_RadiusX", "Terrain8_RadiusY",
    "Terrain8_RatioX", "Terrain8_RatioY", "Terrain8_Smoothing", "Terrain8_Morph", "Terrain8_Drive"
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
