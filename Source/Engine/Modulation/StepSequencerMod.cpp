#include "StepSequencerMod.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
size_t stepIndex(int lane, int step)
{
    const int safeLane = juce::jlimit(0, StepSequencerMod::kNumLanes - 1, lane);
    const int safeStep = juce::jlimit(0, StepSequencerMod::kNumSteps - 1, step);
    return (size_t) (safeLane * StepSequencerMod::kNumSteps + safeStep);
}
}

void StepSequencerMod::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    fallbackSamplesIntoStep = 0.0;
}

void StepSequencerMod::reset()
{
    currentStep.store(-1, std::memory_order_relaxed);
    fallbackSamplesIntoStep = 0.0;
    clearOutput();
}

void StepSequencerMod::setAPVTS(juce::AudioProcessorValueTreeState* state)
{
    apvts = state;
    params = {};
    if (apvts != nullptr)
        buildParamCache();
}

void StepSequencerMod::setHostBPM(double bpm)
{
    if (bpm > 1.0 && std::isfinite(bpm))
        hostBPM = bpm;
}

void StepSequencerMod::setHostPosition(std::optional<double> ppqPosition)
{
    hostPpqPosition = ppqPosition;
}

void StepSequencerMod::processBlock(int numSamples)
{
    if (apvts == nullptr)
        return;

    if (!params.built)
        buildParamCache();

    const auto divisionIndex = loadInt(params.division, 2);
    const auto subdivisionIndex = loadInt(params.subdivision, 2);
    const int sequenceLength = effectiveSequenceLength(
        sequenceLengthForIndex(loadInt(params.length, 1)),
        loadInt(params.loopLength, kNumSteps));
    const double ppqPerStep = ppqForDivisionIndex(divisionIndex)
                            / (double) subdivisionMultiplierForIndex(subdivisionIndex);

    int step = currentStep.load(std::memory_order_relaxed);

    if (hostPpqPosition.has_value() && ppqPerStep > 0.0)
    {
        const int rawStep = (int) std::floor(*hostPpqPosition / ppqPerStep);
        step = ((rawStep % sequenceLength) + sequenceLength) % sequenceLength;
        fallbackSamplesIntoStep = 0.0;
    }
    else
    {
        if (step < 0)
            step = 0;
        else if (step >= sequenceLength)
            step %= sequenceLength;

        const double secondsPerQuarter = 60.0 / juce::jmax(1.0, hostBPM);
        const double samplesPerStep = juce::jmax(1.0, ppqPerStep * secondsPerQuarter * sampleRate);

        fallbackSamplesIntoStep += (double) juce::jmax(0, numSamples);
        while (fallbackSamplesIntoStep >= samplesPerStep)
        {
            fallbackSamplesIntoStep -= samplesPerStep;
            step = (step + 1) % sequenceLength;
        }
    }

    currentStep.store(step, std::memory_order_relaxed);
    clearOutput();

    const float depth = juce::jlimit(0.0f, 1.0f, load(params.depth, 0.45f));
    if (depth <= 0.0f || step < 0)
        return;

    for (int lane = 0; lane < kNumLanes; ++lane)
    {
        const auto target = static_cast<ModDest>(
            juce::jlimit(0, (int) ModDest::NumDests - 1, loadInt(params.targets[(size_t) lane])));

        if (target == ModDest::None)
            continue;

        const float value = juce::jlimit(0.0f, 1.0f, load(params.steps[stepIndex(lane, step)], 0.0f));
        applyTarget(target, value, value * 2.0f - 1.0f, depth);
    }

    for (auto& dest : output.dests)
        dest = juce::jlimit(-4.0f, 4.0f, dest);
}

juce::String StepSequencerMod::stepParamID(int lane, int step)
{
    return "modseq_l" + juce::String(juce::jlimit(0, kNumLanes - 1, lane))
         + "_s" + juce::String(juce::jlimit(0, kNumSteps - 1, step));
}

juce::String StepSequencerMod::laneTargetParamID(int lane)
{
    return "modseq_lane" + juce::String(juce::jlimit(0, kNumLanes - 1, lane)) + "_target";
}

double StepSequencerMod::ppqForDivisionIndex(int index)
{
    static constexpr double divisions[] = {
        1.0,        // 1/4
        0.5,        // 1/8
        0.25,       // 1/16
        0.125,      // 1/32
        0.0625,     // 1/64
        1.0 / 3.0,  // 1/8T
        1.0 / 6.0,  // 1/16T
        0.75,       // 1/8D
        0.375       // 1/16D
    };

    return divisions[(size_t) juce::jlimit(0, (int) std::size(divisions) - 1, index)];
}

int StepSequencerMod::subdivisionMultiplierForIndex(int index)
{
    static constexpr int multipliers[] = { 1, 2, 4, 8 };
    return multipliers[(size_t) juce::jlimit(0, (int) std::size(multipliers) - 1, index)];
}

int StepSequencerMod::sequenceLengthForIndex(int index)
{
    static constexpr int lengths[] = { 8, 16, 24, 32 };
    return lengths[(size_t) juce::jlimit(0, (int) std::size(lengths) - 1, index)];
}

int StepSequencerMod::effectiveSequenceLength(int sequenceLength, int loopLength)
{
    const int safeSequenceLength = juce::jlimit(1, kNumSteps, sequenceLength);
    const int safeLoopLength = juce::jlimit(1, kNumSteps, loopLength);
    return juce::jlimit(1, safeSequenceLength, safeLoopLength);
}

void StepSequencerMod::buildParamCache()
{
    if (apvts == nullptr)
        return;

    params.depth = apvts->getRawParameterValue(kDepthParamID);
    params.division = apvts->getRawParameterValue(kDivisionParamID);
    params.subdivision = apvts->getRawParameterValue(kSubdivisionParamID);
    params.length = apvts->getRawParameterValue(kLengthParamID);
    params.loopLength = apvts->getRawParameterValue(kLoopLengthParamID);

    for (int lane = 0; lane < kNumLanes; ++lane)
    {
        params.targets[(size_t) lane] = apvts->getRawParameterValue(laneTargetParamID(lane));
        for (int step = 0; step < kNumSteps; ++step)
            params.steps[stepIndex(lane, step)] = apvts->getRawParameterValue(stepParamID(lane, step));
    }

    params.built = true;
}

void StepSequencerMod::clearOutput()
{
    std::fill(std::begin(output.sources), std::end(output.sources), 0.0f);
    std::fill(std::begin(output.dests), std::end(output.dests), 0.0f);
}

void StepSequencerMod::applyTarget(ModDest target, float unipolar, float bipolar, float depth)
{
    const float amount = (targetUsesBipolar(target) ? bipolar : unipolar) * depth;
    output.dests[(int) target] += amount;
}

float StepSequencerMod::load(std::atomic<float>* value, float fallback) noexcept
{
    return value != nullptr ? value->load(std::memory_order_relaxed) : fallback;
}

int StepSequencerMod::loadInt(std::atomic<float>* value, int fallback) noexcept
{
    return value != nullptr ? (int) value->load(std::memory_order_relaxed) : fallback;
}

bool StepSequencerMod::targetUsesBipolar(ModDest target) noexcept
{
    return target == ModDest::OscPitch_All
        || target == ModDest::WT_Position
        || target == ModDest::FilterA_Cutoff
        || target == ModDest::FilterB_Cutoff
        || target == ModDest::Master_Pan
        || target == ModDest::Gran_Position
        || target == ModDest::Gran_Pitch
        || target == ModDest::Terrain_CenterX
        || target == ModDest::Terrain_CenterY
        || target == ModDest::Terrain_RadiusX
        || target == ModDest::Terrain_RadiusY
        || target == ModDest::Terrain_RatioX
        || target == ModDest::Terrain_RatioY;
}
