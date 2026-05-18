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
    RolandJuno106,
    RolandJuno60,
    RolandJupiter4,
    RolandJupiter8,
    TB303,
    SequentialP5Rev2,
    SequentialP5Rev3,
    SequentialP600,
    SequentialP6,
    SequentialProOne,
    GliesePrism,
    AltLadder,
    PhysicalLadder,
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
