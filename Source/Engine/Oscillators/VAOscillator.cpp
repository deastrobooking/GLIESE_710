#include "VAOscillator.h"

void VAOscillator::prepare(double sampleRate)
{
    sr = sampleRate;
    rebuildVoiceState();
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
    params.shapeMorph = params.shapeMorph < 0.f
        ? (float)static_cast<int>(params.shape)
        : juce::jlimit(0.f, (float)((int)Shape::NumShapes - 1), params.shapeMorph);
    params.pulseWidth = juce::jlimit(0.05f, 0.95f, params.pulseWidth);
    params.ringMod = juce::jlimit(0.f, 1.f, params.ringMod);
    params.wavefold = juce::jlimit(0.f, 1.f, params.wavefold);
    rebuildVoiceState();
}

void VAOscillator::setFrequency(float hz)
{
    freq = juce::jlimit(20.f, (float)(sr * 0.49), hz);
    rebuildVoiceState();
}

void VAOscillator::rebuildVoiceState()
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

        float detuneSemitones = (n == 1) ? 0.f
            : ((float)i / (float)(n - 1) * 2.f - 1.f) * params.detune * 0.5f;
        float voiceFreq = freq * std::pow(2.f, detuneSemitones / 12.f);
        phaseIncs[i] = (float)(voiceFreq / sr);
    }

    outputScale = (n > 1) ? 1.f / std::sqrt((float)n) : 1.f;
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

float VAOscillator::generateSaw(float phase, float phaseInc) const
{
    float v = 2.f * phase - 1.f;
    v -= polyBlep(phase, phaseInc);
    return v;
}

float VAOscillator::generateSquare(float phase, float phaseInc) const
{
    float pw = params.pulseWidth;
    float v  = phase < pw ? 1.f : -1.f;
    v += polyBlep(phase, phaseInc);

    // blep at pw boundary
    float shifted = phase - pw;
    if (shifted < 0.f) shifted += 1.f;
    v -= polyBlep(shifted, phaseInc);
    return v;
}

float VAOscillator::generateTriangle(float phase) const
{
    return 2.f * std::abs(2.f * phase - 1.f) - 1.f;
}

float VAOscillator::generateShape(int shapeIndex, float phase, float phaseInc) const
{
    switch ((Shape)juce::jlimit(0, (int)Shape::NumShapes - 1, shapeIndex))
    {
        case Shape::Sine:
            return std::sin(phase * juce::MathConstants<float>::twoPi);
        case Shape::Saw:
            return generateSaw(phase, phaseInc);
        case Shape::RevSaw:
            return -generateSaw(phase, phaseInc);
        case Shape::Square:
            return generateSquare(phase, phaseInc);
        case Shape::Triangle:
            return generateTriangle(phase);
        case Shape::Pulse:
            return generateSquare(phase, phaseInc);
        case Shape::NumShapes:
        default:
            return 0.f;
    }
}

float VAOscillator::generateMorphedShape(float phase, float phaseInc) const
{
    float shapePos = juce::jlimit(0.f, (float)((int)Shape::NumShapes - 1), params.shapeMorph);
    int lower = (int)std::floor(shapePos);
    int upper = juce::jmin(lower + 1, (int)Shape::NumShapes - 1);
    float mix = shapePos - (float)lower;

    float a = generateShape(lower, phase, phaseInc);
    float b = generateShape(upper, phase, phaseInc);
    return a + (b - a) * mix;
}

float VAOscillator::foldSignal(float x) noexcept
{
    x = std::fmod(x + 1.f, 4.f);
    if (x < 0.f)
        x += 4.f;

    return x <= 2.f ? x - 1.f : 3.f - x;
}

std::pair<float, float> VAOscillator::processSample()
{
    float L = 0.f, R = 0.f;
    int   n = params.unisonCount;

    for (int i = 0; i < n; ++i)
    {
        float phaseInc = phaseIncs[i];
        float sample = generateMorphedShape(phases[i], phaseInc);

        if (params.ringMod > 0.f)
        {
            float ring = std::sin(phases[i] * juce::MathConstants<float>::twoPi * 2.f);
            sample = sample + (sample * ring - sample) * params.ringMod;
        }

        if (params.wavefold > 0.f)
        {
            float folded = foldSignal(sample * (1.f + params.wavefold * 8.f));
            sample = sample + (folded - sample) * params.wavefold;
        }

        phases[i] += phaseInc;
        if (phases[i] >= 1.f) phases[i] -= 1.f;

        L += sample * panWeights[i][0];
        R += sample * panWeights[i][1];
    }

    return { L * outputScale, R * outputScale };
}
