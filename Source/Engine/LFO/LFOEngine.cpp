#include "LFOEngine.h"
#include <cmath>

static size_t lfoIndex(int i)
{
    return static_cast<size_t>(juce::jlimit(0, LFOEngine::kNumLFOs - 1, i));
}

void LFOEngine::prepare(double sampleRate)
{
    sr = sampleRate;
    reset();
}

void LFOEngine::setHostBPM(double beatsPerMinute)
{
    bpm = juce::jlimit(20.0, 300.0, beatsPerMinute);
}

void LFOEngine::setParams(int i, const LFOParams& p)
{
    jassert(i >= 0 && i < kNumLFOs);
    params[lfoIndex(i)] = p;
}

void LFOEngine::noteOn()
{
    for (size_t i = 0; i < params.size(); ++i)
        if (params[i].retrig)
            phases[i] = params[i].phase;
}

void LFOEngine::reset()
{
    phases.fill(0.f);
    values.fill(0.f);
    shPrev.fill(0.f);
    seeds = { 1u, 2u, 3u, 4u };
}

float LFOEngine::whiteSample(int i)
{
    auto idx = lfoIndex(i);
    seeds[idx] = seeds[idx] * 1664525u + 1013904223u;
    return ((float)(seeds[idx] >> 16) / 32767.5f) - 1.f;
}

float LFOEngine::lfoRateHz(int i) const
{
    const auto& p = params[lfoIndex(i)];
    if (!p.sync || bpm <= 0.0)
        return juce::jlimit(0.01f, 100.f, p.rate);

    float syncPos = juce::jlimit(0.f, (float)(kNumSyncRates - 1), p.syncRate);
    int lower = (int)std::floor(syncPos);
    int upper = juce::jmin(lower + 1, kNumSyncRates - 1);
    float mix = syncPos - (float)lower;
    float beats = std::exp2(std::log2(kSyncBeats[(size_t)lower])
                            + (std::log2(kSyncBeats[(size_t)upper])
                               - std::log2(kSyncBeats[(size_t)lower])) * mix);
    float hz     = (float)(bpm / 60.0) / beats;
    return juce::jlimit(0.01f, 200.f, hz);
}

float LFOEngine::generateShape(int i, float ph)
{
    // ph is 0..1
    auto idx = lfoIndex(i);
    auto& p = params[idx];
    switch (p.shape)
    {
        case Shape::Sine:
            return std::sin(ph * juce::MathConstants<float>::twoPi);

        case Shape::Triangle:
            return (ph < 0.5f) ? (4.f * ph - 1.f) : (3.f - 4.f * ph);

        case Shape::Saw:
            return 2.f * ph - 1.f;

        case Shape::RevSaw:
            return 1.f - 2.f * ph;

        case Shape::Square:
            return (ph < 0.5f) ? 1.f : -1.f;

        case Shape::SampleHold:
        {
            // Generate new value on phase wrap (handled in processBlock)
            return shPrev[idx];
        }

        case Shape::SmoothSH:
        {
            float target = shPrev[idx];
            float smooth = 0.01f;   // lag coefficient
            values[idx] = values[idx] + smooth * (target - values[idx]);
            return values[idx];
        }

        default:
            return 0.f;
    }
}

void LFOEngine::processBlock(int numSamples)
{
    for (int sample = 0; sample < numSamples; ++sample)
        processSample();
}

void LFOEngine::processSample()
{
    for (int i = 0; i < kNumLFOs; ++i)
    {
        auto idx = lfoIndex(i);
        float hz       = lfoRateHz(i);
        float phaseInc = hz / (float)sr;
        float ph       = phases[idx] + params[idx].phase;
        if (ph >= 1.f)
            ph -= std::floor(ph);

        const float advanced = ph + phaseInc;
        const int wraps = static_cast<int>(advanced);
        ph = advanced - (float)wraps;

        if (wraps > 0
            && (params[idx].shape == Shape::SampleHold || params[idx].shape == Shape::SmoothSH))
        {
            shPrev[idx] = whiteSample(i);
        }

        phases[idx] = ph - params[idx].phase;
        if (phases[idx] < 0.f) phases[idx] += 1.f;

        values[idx] = generateShape(i, ph) * params[idx].amount;
    }
}
