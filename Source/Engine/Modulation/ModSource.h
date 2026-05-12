#pragma once

enum class ModSource : int
{
    None = 0,
    LFO1, LFO2, LFO3, LFO4,
    Env_Master, Env_3D, Env_FM, Env_WT, Env_VA, Env_FilterA, Env_FilterB,
    Velocity, Aftertouch, ModWheel, PitchBend, CC74, ChanPressure,
    Macro1, Macro2, Macro3, Macro4, Macro5, Macro6, Macro7, Macro8,
    NumSources
};
