#pragma once

#include <cmath>

namespace gliese::dsp
{
inline float pitchRatioFromSemitones(float semitones) noexcept
{
    return std::exp2(semitones / 12.f);
}

inline float pitchRatioFromCents(float cents) noexcept
{
    return std::exp2(cents / 1200.f);
}
}
