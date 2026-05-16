#pragma once

enum class FilterMode
{
    LP12 = 0, LP24,
    HP12,     HP24,
    Bandpass,
    Notch,
    Peak,
    LowShelf,
    HighShelf,
    Comb,
    AM_Plus,   // signal * (1 + mod)
    AM_Minus,  // signal * mod
    Ladder12,
    Ladder18,
    Ladder24,
    SEMMorph,
    MSDual,
    Formant,
    NumModes
};

enum class FilterRouting
{
    Series = 0,
    Parallel,
    AOnly,
    BOnly,
    NumRoutings
};
