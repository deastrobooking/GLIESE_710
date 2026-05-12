#include "ModMatrix.h"
#include <JuceHeader.h>

static bool sourceIsNaturallyBipolar(ModSource source)
{
    return source == ModSource::LFO1
        || source == ModSource::LFO2
        || source == ModSource::LFO3
        || source == ModSource::LFO4
        || source == ModSource::PitchBend;
}

void ModMatrix::setSlot(int index, const ModSlot& slot)
{
    jassert(index >= 0 && index < kNumSlots);
    slots[(size_t) juce::jlimit(0, kNumSlots - 1, index)] = slot;
}

void ModMatrix::clear()
{
    for (auto& s : slots) s = {};
}

void ModMatrix::process(const ModValues& srcVals, ModValues& dstVals) const
{
    // Zero destinations before accumulation
    std::fill(std::begin(dstVals.dests), std::end(dstVals.dests), 0.f);

    for (const auto& slot : slots)
    {
        if (slot.source == ModSource::None || slot.dest == ModDest::None)
            continue;
        if (std::abs(slot.amount) < 1e-6f)
            continue;

        float srcVal = srcVals.sources[(int)slot.source];

        // Convert unipolar sources to -1..1 if bipolar mode.
        if (slot.bipolar && !sourceIsNaturallyBipolar(slot.source))
            srcVal = srcVal * 2.f - 1.f;

        dstVals.dests[(int)slot.dest] += srcVal * slot.amount;
    }

    // Clamp destinations to sane range
    for (auto& d : dstVals.dests)
        d = juce::jlimit(-4.f, 4.f, d);
}
