#pragma once
#include <JuceHeader.h>

class MasterOutputStage
{
public:
    struct Result
    {
        float peakL = 0.f;
        float peakR = 0.f;
        bool limited = false;
        bool recoveredNonFinite = false;
    };

    void prepare(double sampleRate);
    void reset() noexcept;
    Result process(juce::AudioBuffer<float>& buffer) noexcept;

private:
    static constexpr float kCeiling = 0.98f;
    double sampleRate = 48000.0;
    float gain = 1.f;
    float releaseCoeff = 0.999f;
};
