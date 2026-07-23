#include "MasterOutputStage.h"
#include <cmath>

void MasterOutputStage::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    releaseCoeff = std::exp(-1.f / (0.08f * (float) sampleRate));
    reset();
}

void MasterOutputStage::reset() noexcept
{
    gain = 1.f;
}

MasterOutputStage::Result MasterOutputStage::process(juce::AudioBuffer<float>& buffer) noexcept
{
    Result result;
    if (buffer.getNumChannels() < 1)
        return result;

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float l = left[sample];
        float r = right[sample];
        if (!std::isfinite(l))
        {
            l = 0.f;
            result.recoveredNonFinite = true;
        }
        if (!std::isfinite(r))
        {
            r = 0.f;
            result.recoveredNonFinite = true;
        }

        const float inputPeak = juce::jmax(std::abs(l), std::abs(r));
        const float targetGain = inputPeak > kCeiling ? kCeiling / inputPeak : 1.f;
        if (targetGain < gain)
            gain = targetGain;
        else
            gain = 1.f + releaseCoeff * (gain - 1.f);

        result.limited = result.limited || targetGain < 1.f;
        l *= gain;
        r *= gain;
        left[sample] = l;
        right[sample] = r;
        result.peakL = juce::jmax(result.peakL, std::abs(l));
        result.peakR = juce::jmax(result.peakR, std::abs(r));
    }

    return result;
}
