#pragma once
#include "ModSource.h"
#include "ModDest.h"
#include <array>

struct ModSlot
{
    ModSource source  = ModSource::None;
    ModDest   dest    = ModDest::None;
    float     amount  = 0.f;   // -1..1
    bool      bipolar = true;  // if false, unipolar (source 0..1)
};

struct ModValues
{
    float sources[(int)ModSource::NumSources] = {};
    float dests  [(int)ModDest::NumDests]     = {};
};

class ModMatrix
{
public:
    static constexpr int kNumSlots = 32;

    void setSlot(int index, const ModSlot& slot);
    const ModSlot& getSlot(int index) const { return slots[index]; }

    // Accumulate all mod destinations from source values
    void process(const ModValues& sourceValues, ModValues& destValues) const;

    void clear();

private:
    std::array<ModSlot, kNumSlots> slots;
};
