#pragma once

#include "ModMatrix.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <optional>

class StepSequencerMod
{
public:
    static constexpr int kNumLanes = 4;
    static constexpr int kNumSteps = 32;
    static constexpr int kSegmentLength = 8;
    static constexpr int kNumSegments = kNumSteps / kSegmentLength;

    static constexpr const char* kDepthParamID       = "modseq_depth";
    static constexpr const char* kDivisionParamID    = "modseq_division";
    static constexpr const char* kSubdivisionParamID = "modseq_subdivision";
    static constexpr const char* kLengthParamID      = "modseq_length";

    void prepare(double newSampleRate);
    void reset();
    void setAPVTS(juce::AudioProcessorValueTreeState* state);
    void setHostBPM(double bpm);
    void setHostPosition(std::optional<double> ppqPosition);
    void processBlock(int numSamples);

    const ModValues& getOutput() const noexcept { return output; }
    int getCurrentStep() const noexcept { return currentStep.load(std::memory_order_relaxed); }

    static juce::String stepParamID(int lane, int step);
    static juce::String laneTargetParamID(int lane);
    static double ppqForDivisionIndex(int index);
    static int subdivisionMultiplierForIndex(int index);
    static int sequenceLengthForIndex(int index);

private:
    struct ParamCache
    {
        std::atomic<float>* depth       = nullptr;
        std::atomic<float>* division    = nullptr;
        std::atomic<float>* subdivision = nullptr;
        std::atomic<float>* length      = nullptr;
        std::array<std::atomic<float>*, kNumLanes * kNumSteps> steps {};
        std::array<std::atomic<float>*, kNumLanes> targets {};
        bool built = false;
    };

    juce::AudioProcessorValueTreeState* apvts = nullptr;
    ParamCache params;
    ModValues output;
    std::atomic<int> currentStep { -1 };

    double sampleRate = 48000.0;
    double hostBPM = 120.0;
    std::optional<double> hostPpqPosition;
    double fallbackSamplesIntoStep = 0.0;

    void buildParamCache();
    void clearOutput();
    void applyTarget(ModDest target, float unipolar, float bipolar, float depth);

    static float load(std::atomic<float>* value, float fallback = 0.0f) noexcept;
    static int loadInt(std::atomic<float>* value, int fallback = 0) noexcept;
    static bool targetUsesBipolar(ModDest target) noexcept;
};
