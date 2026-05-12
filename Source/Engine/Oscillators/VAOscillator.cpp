#include "VAOscillator.h"

void VAOscillator::prepare(double sampleRate)
{
    sr = sampleRate;
    reset();
}

void VAOscillator::reset()
{
    std::fill(std::begin(phases), std::end(phases), 0.f);
    std::fill(std::begin(triIntegrator), std::end(triIntegrator), 0.f);
}

void VAOscillator::setParams(const Params& p)
{
    params = p;
    params.unisonCount = juce::jlimit(1, kMaxUnison, params.unisonCount);
    rebuildPanWeights();
}

void VAOscillator::setFrequency(float hz)
{
    freq = hz;
}

void VAOscillator::rebuildPanWeights()
{
    int n = params.unisonCount;
    for (int i = 0; i < n; ++i)
    {
        // pan evenly across stereo field scaled by spread
        float pan = (n == 1) ? 0.f
                              : ((float)i / (float)(n - 1) * 2.f - 1.f) * params.spread;
        // constant-power panning
        float angle = (pan + 1.f) * 0.25f * juce::MathConstants<float>::pi;
        panWeights[i][0] = std::cos(angle);
        panWeights[i][1] = std::sin(angle);
    }
}

float VAOscillator::polyBlep(float phase, float phaseInc)
{
    if (phase < phaseInc)
    {
        float t = phase / phaseInc;
        return t + t - t * t - 1.f;
    }
    if (phase > 1.f - phaseInc)
    {
        float t = (phase - 1.f) / phaseInc;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

float VAOscillator::generateSaw(float& phase, float phaseInc) const
{
    float v = 2.f * phase - 1.f;
    v -= polyBlep(phase, phaseInc);
    return v;
}

float VAOscillator::generateSquare(float& phase, float phaseInc) const
{
    float pw = juce::jlimit(0.05f, 0.95f, params.pulseWidth);
    float v  = phase < pw ? 1.f : -1.f;
    v += polyBlep(phase, phaseInc);

    // blep at pw boundary
    float shifted = phase - pw;
    if (shifted < 0.f) shifted += 1.f;
    v -= polyBlep(shifted, phaseInc);
    return v;
}

float VAOscillator::generateTriangle(float& phase, float phaseInc)
{
    // Naive square -> integrate -> triangle (BLAMP)
    float square = generateSquare(phase, phaseInc);
    // Simple leaky integrator approximation
    float tri = phaseInc * square + (1.f - phaseInc) * 0.f; // placeholder
    // Production: use per-voice integrator
    return square; // TODO: replace with proper BLAMP triangle
}

std::pair<float, float> VAOscillator::processSample()
{
    float L = 0.f, R = 0.f;
    int   n = params.unisonCount;

    for (int i = 0; i < n; ++i)
    {
        // Each unison voice gets a detuned frequency
        float detuneSemitones = (n == 1) ? 0.f
            : ((float)i / (float)(n - 1) * 2.f - 1.f) * params.detune * 0.5f;
        float voiceFreq    = freq * std::pow(2.f, detuneSemitones / 12.f);
        float phaseInc     = (float)(voiceFreq / sr);

        float sample = 0.f;
        switch (params.shape)
        {
            case Shape::Sine:
                sample = std::sin(phases[i] * juce::MathConstants<float>::twoPi);
                break;
            case Shape::Saw:
                sample = generateSaw(phases[i], phaseInc);
                break;
            case Shape::RevSaw:
                sample = -generateSaw(phases[i], phaseInc);
                break;
            case Shape::Square:
            case Shape::Pulse:
                sample = generateSquare(phases[i], phaseInc);
                break;
            case Shape::Triangle:
                sample = generateTriangle(phases[i], phaseInc);
                break;
        }

        phases[i] += phaseInc;
        if (phases[i] >= 1.f) phases[i] -= 1.f;

        L += sample * panWeights[i][0];
        R += sample * panWeights[i][1];
    }

    float scale = (n > 1) ? 1.f / std::sqrt((float)n) : 1.f;
    return { L * scale, R * scale };
}
