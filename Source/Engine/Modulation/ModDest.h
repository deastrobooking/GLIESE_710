#pragma once

enum class ModDest : int
{
    None = 0,
    OscPitch_All,
    Osc3D_Radius, Osc3D_Depth, Osc3D_Warp, Osc3D_Chaos, Osc3D_Morph, Osc3D_Level,
    FM_Op0_Level, FM_Op1_Level, FM_Op2_Level, FM_Op3_Level, FM_Op4_Level,
    FM_Op5_Level, FM_Op6_Level, FM_Op7_Level, FM_Op8_Level, FM_Op9_Level,
    FM_Op0_Ratio, FM_Op1_Ratio, FM_Op2_Ratio, FM_Op3_Ratio, FM_Op4_Ratio,
    WT_Position, WT_Level,
    VA_PW, VA_Detune, VA_Spread, VA_Level,
    FilterA_Cutoff, FilterA_Resonance, FilterA_Drive,
    FilterB_Cutoff, FilterB_Resonance, FilterB_Drive,
    LFO1_Rate, LFO2_Rate, LFO3_Rate, LFO4_Rate,
    LFO1_Amount, LFO2_Amount, LFO3_Amount, LFO4_Amount,
    Master_Volume, Master_Pan,
    Arp_Gate, Arp_Rate,
    Osc3D_FieldFreq, Osc3D_ZRatio, Osc3D_Fold, Osc3D_Drive,
    NumDests
};
