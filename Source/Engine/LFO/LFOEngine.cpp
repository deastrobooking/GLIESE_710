#include "LFOEngine.h"

constexpr float LFOEngine::kSyncBeats[LFOEngine::kNumSyncRates];

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
    params[i] = p;
}

void LFOEngine::noteOn()
{
    for (int i = 0; i < kNumLFOs; ++i)
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
    seeds[i] = seeds[i] * 1664525u + 1013904223u;
    return ((float)(seeds[i] >> 16) / 32767.5f) - 1.f;
}

float LFOEngine::lfoRateHz(int i) const
{
    const auto& p = params[i];
    if (p.syncIdx == 0 || bpm <= 0.0)
        return juce::jlimit(0.01f, 100.f, p.rate);

    float beats  = kSyncBeats[juce::jlimit(0, kNumSyncRates - 1, p.syncIdx)];
    float hz     = (float)(bpm / 60.0) / beats;
    return juce::jlimit(0.01f, 200.f, hz);
}

float LFOEngine::generateShape(int i, float ph)
{
    // ph is 0..1
    auto& p = params[i];
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
            return shPrev[i];
        }

        case Shape::SmoothSH:
        {
            float target = shPrev[i];
            float smooth = 0.01f;   // lag coefficient
            values[i] = values[i] + smooth * (target - values[i]);
            return values[i];
        }

        default:
            return 0.f;
    }
}

void LFOEngine::processBlock(int numSamples)
{
    for (int i = 0; i < kNumLFOs; ++i)
    {
        float hz       = lfoRateHz(i);
        float phaseInc = hz / (float)sr;
        float ph       = phases[i] + params[i].phase;
        if (ph >= 1.f) ph -= 1.f;

        // Accumulate last sample of block
        for (int s = 0; s < numSamples; ++s)
        {
            float prevPh = ph;
            ph += phaseInc;

            if (ph >= 1.f)
            {
                ph -= 1.f;
                // Phase wrap: new S&H value
                if (params[i].shape == Shape::SampleHold || params[i].shape == Shape::SmoothSH)
                    shPrev[i] = whiteSample(i);
            }
        }

        phases[i] = ph - params[i].phase;
        if (phases[i] < 0.f) phases[i] += 1.f;

        values[i] = generateShape(i, ph) * params[i].amount;
    }
}
