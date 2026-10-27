#include "KarplusStrong.h"
#include <cmath>

void KarplusStrong::prepare(double sampleRate)
{
    sr     = sampleRate;
    bufLen = kMaxDelaySamples;
    reset();
}

void KarplusStrong::reset()
{
    std::fill(std::begin(delayBuffer), std::end(delayBuffer), 0.0f);
    writeIdx    = 0;
    lastLPFOut  = 0.0f;
    lastAPFIn   = 0.0f;
    lastAPFOut  = 0.0f;
    active      = false;
    quietSamples = 0;
}

void KarplusStrong::setFrequency(float hz)
{
    frequency = juce::jlimit(20.0f, 20000.0f, hz);
}

float KarplusStrong::nextNoise()
{
    noiseSeed = noiseSeed * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int32_t>(noiseSeed)) / static_cast<float>(0x80000000u);
}

float KarplusStrong::readInterpolated(float delaySamples) const
{
    int n    = juce::jlimit(1, bufLen - 2, static_cast<int>(delaySamples));
    float frac = delaySamples - static_cast<float>(n);
    int r0   = (writeIdx - n + bufLen) % bufLen;
    int r1   = (writeIdx - n - 1 + bufLen) % bufLen;
    return delayBuffer[r0] * (1.f - frac) + delayBuffer[r1] * frac;
}

void KarplusStrong::pluck(float velocity, const Params& params)
{
    double periodSamples = sr / std::max(1.0, static_cast<double>(frequency));
    int pluckLen = juce::jlimit(2, bufLen - 1, static_cast<int>(periodSamples));

    for (int i = 0; i < pluckLen; ++i)
    {
        float noise       = nextNoise();
        float glitch      = ((i % 4) > 1) ? 1.0f : -1.0f;
        float excitation  = noise + params.morph * (glitch - noise);

        int combDel  = juce::jlimit(1, pluckLen - 1, static_cast<int>(pluckLen * params.position));
        int prevIdx  = (writeIdx - combDel + bufLen) % bufLen;
        excitation  -= 0.5f * delayBuffer[prevIdx];

        delayBuffer[writeIdx] = excitation * velocity;
        if (++writeIdx >= bufLen)
            writeIdx = 0;
    }

    lastLPFOut = 0.0f;
    lastAPFIn  = 0.0f;
    lastAPFOut = 0.0f;
    active     = true;
    quietSamples = 0;
}

float KarplusStrong::processSample(const Params& params)
{
    if (!active)
        return 0.0f;

    double periodSamples = sr / std::max(1.0, static_cast<double>(frequency));
    float  readDelay     = juce::jlimit(1.0f, static_cast<float>(bufLen - 2),
                                        static_cast<float>(periodSamples) - 0.5f);

    float s = readInterpolated(readDelay);

    // Stiffness all-pass (first-order APF)
    float g      = params.stiffness;
    float apfOut = g == 0.f ? s : -g * s + lastAPFIn + g * lastAPFOut;
    lastAPFIn    = s;
    lastAPFOut   = apfOut;
    s            = apfOut;

    // LPF blend
    float lpfOut = s * params.brightness + lastLPFOut * (1.f - params.brightness);
    lastLPFOut   = lpfOut;

    // Apply decay
    float out = lpfOut * params.decay;

    // Write back
    delayBuffer[writeIdx] = out;
    if (++writeIdx >= bufLen)
        writeIdx = 0;

    // Silence check
    quietSamples = std::abs(out) < 1e-6f ? quietSamples + 1 : 0;
    if (quietSamples > (int)std::ceil(periodSamples) + 2)
        active = false;

    return out;
}
