#include "VAOscillator.h"
#include "../DSPUtils.h"

void VAOscillator::prepare(double sampleRate)
{
    sr = sampleRate;
    rebuildVoiceState();
    reset();
}

void VAOscillator::reset()
{
    std::fill(std::begin(phases), std::end(phases), 0.f);
    std::fill(std::begin(auxPhases), std::end(auxPhases), 0.173f);
    std::fill(std::begin(subPhases), std::end(subPhases), 0.f);
    std::fill(std::begin(hpLastIn), std::end(hpLastIn), 0.f);
    std::fill(std::begin(hpLastOut), std::end(hpLastOut), 0.f);
    std::fill(std::begin(triIntegrator), std::end(triIntegrator), 0.f);

    for (int i = 0; i < kMaxUnison; ++i)
        driftPhases[i] = std::fmod(0.137f * (float)(i + 1), 1.f);
}

void VAOscillator::setParams(const Params& p)
{
    params = p;
    params.model = static_cast<Model>(juce::jlimit(0, (int)Model::NumModels - 1, (int)params.model));
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
        float voiceFreq = freq * gliese::dsp::pitchRatioFromSemitones(detuneSemitones)
                         * centsToRatio(modelCalibrationCents(i));
        phaseIncs[i] = (float)(voiceFreq / sr);

        float auxCents = 0.f;
        switch (params.model)
        {
            case Model::Oberheim:   auxCents = 6.5f + 0.4f * (float)(i % 3); break;
            case Model::Sequential: auxCents = 3.0f + 0.25f * (float)((i + 1) % 4); break;
            case Model::Moog:       auxCents = -1.2f; break;
            case Model::Roland:
            case Model::Native:
            case Model::NumModels:  break;
        }

        auxPhaseIncs[i] = (float)(voiceFreq * centsToRatio(auxCents) / sr);
        subPhaseIncs[i] = (float)(voiceFreq * 0.5f / sr);
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
    return generatePulse(phase, phaseInc, params.pulseWidth);
}

float VAOscillator::generatePulse(float phase, float phaseInc, float pulseWidth) const
{
    float pw = juce::jlimit(0.05f, 0.95f, pulseWidth);
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
    return generateShapeAt(params.shapeMorph, params.pulseWidth, phase, phaseInc);
}

float VAOscillator::generateShapeAt(float shapeMorph, float pulseWidth, float phase, float phaseInc) const
{
    float shapePos = juce::jlimit(0.f, (float)((int)Shape::NumShapes - 1), shapeMorph);
    int lower = (int)std::floor(shapePos);
    int upper = juce::jmin(lower + 1, (int)Shape::NumShapes - 1);
    float mix = shapePos - (float)lower;

    auto shapeAtIndex = [this, pulseWidth, phase, phaseInc](int shapeIndex)
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
                return generatePulse(phase, phaseInc, 0.5f);
            case Shape::Triangle:
                return generateTriangle(phase);
            case Shape::Pulse:
                return generatePulse(phase, phaseInc, pulseWidth);
            case Shape::NumShapes:
            default:
                return 0.f;
        }
    };

    float a = shapeAtIndex(lower);
    float b = shapeAtIndex(upper);
    return a + (b - a) * mix;
}

float VAOscillator::applyModelShape(float x, int voiceIndex) noexcept
{
    switch (params.model)
    {
        case Model::Moog:
        {
            x = asymmetry(x, 0.055f);
            x = softClip(x * 1.08f, 0.12f);
            return juce::jlimit(-1.35f, 1.35f, x * 0.92f);
        }

        case Model::Roland:
        {
            x = highPassLean(x, voiceIndex);
            return softClip(x, 0.035f) * 0.96f;
        }

        case Model::Oberheim:
        {
            x = asymmetry(x, 0.025f);
            return softClip(x * 1.04f, 0.07f) * 0.9f;
        }

        case Model::Sequential:
        {
            x = asymmetry(x, 0.015f);
            return softClip(x * 1.02f, 0.055f) * 0.94f;
        }

        case Model::Native:
        case Model::NumModels:
        default:
            return x;
    }
}

float VAOscillator::highPassLean(float x, int voiceIndex) noexcept
{
    const int i = juce::jlimit(0, kMaxUnison - 1, voiceIndex);
    const float y = x - hpLastIn[i] + 0.995f * hpLastOut[i];
    hpLastIn[i] = x;
    hpLastOut[i] = y;
    return y;
}

float VAOscillator::modelDriftRatio(int voiceIndex) noexcept
{
    if (params.model == Model::Native)
        return 1.f;

    const int i = juce::jlimit(0, kMaxUnison - 1, voiceIndex);
    float rateHz = 0.11f;
    float cents = 0.f;

    switch (params.model)
    {
        case Model::Moog:       rateHz = 0.075f + 0.011f * (float)i; cents = 1.45f; break;
        case Model::Roland:     rateHz = 0.045f + 0.006f * (float)i; cents = 0.18f; break;
        case Model::Oberheim:   rateHz = 0.095f + 0.013f * (float)i; cents = 0.85f; break;
        case Model::Sequential: rateHz = 0.065f + 0.009f * (float)i; cents = 0.55f; break;
        case Model::Native:
        case Model::NumModels:  break;
    }

    driftPhases[i] += rateHz / (float)sr;
    driftPhases[i] -= std::floor(driftPhases[i]);

    const float wobble = std::sin(driftPhases[i] * juce::MathConstants<float>::twoPi)
                       + 0.35f * std::sin((driftPhases[i] * 0.37f + 0.11f * (float)i)
                                          * juce::MathConstants<float>::twoPi);
    return centsToRatio(cents * wobble);
}

float VAOscillator::modelCalibrationCents(int voiceIndex) const noexcept
{
    if (params.model == Model::Native)
        return 0.f;

    static constexpr float offsets[kMaxUnison] {
        -0.72f, 0.41f, -0.18f, 0.93f, -0.56f, 0.24f, 0.68f, -0.35f
    };

    float scale = 1.f;
    switch (params.model)
    {
        case Model::Moog:       scale = 1.65f; break;
        case Model::Roland:     scale = 0.28f; break;
        case Model::Oberheim:   scale = 1.25f; break;
        case Model::Sequential: scale = 0.95f; break;
        case Model::Native:
        case Model::NumModels:  scale = 0.f; break;
    }

    return offsets[(size_t)juce::jlimit(0, kMaxUnison - 1, voiceIndex)] * scale;
}

float VAOscillator::softClip(float x, float drive) noexcept
{
    const float g = 1.f + juce::jlimit(0.f, 1.f, drive) * 8.f;
    return std::tanh(x * g) / std::tanh(g);
}

float VAOscillator::asymmetry(float x, float amount) noexcept
{
    return x + amount * (x * x - 0.3333333f);
}

float VAOscillator::centsToRatio(float cents) noexcept
{
    return gliese::dsp::pitchRatioFromCents(cents);
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
        float phaseInc = phaseIncs[i] * modelDriftRatio(i);
        float sample = generateMorphedShape(phases[i], phaseInc);

        switch (params.model)
        {
            case Model::Moog:
                sample *= 1.04f;
                break;

            case Model::Roland:
            {
                const float sub = subPhases[i] < 0.5f ? 1.f : -1.f;
                sample = sample * 0.84f + sub * 0.26f;
                break;
            }

            case Model::Oberheim:
            {
                const float auxPw = juce::jlimit(0.08f, 0.92f,
                    params.pulseWidth + ((i & 1) == 0 ? 0.075f : -0.075f));
                const float auxPhase = std::fmod(auxPhases[i] + 0.083f, 1.f);
                const float aux = generateShapeAt(params.shapeMorph, auxPw, auxPhase, auxPhaseIncs[i]);
                sample = sample * 0.74f + aux * 0.36f;
                break;
            }

            case Model::Sequential:
            {
                const float auxSaw = generateSaw(auxPhases[i], auxPhaseIncs[i]);
                const float auxTri = generateTriangle(std::fmod(auxPhases[i] + 0.25f, 1.f));
                sample = sample * 0.78f + auxSaw * 0.16f + auxTri * 0.10f;
                break;
            }

            case Model::Native:
            case Model::NumModels:
            default:
                break;
        }

        sample = applyModelShape(sample, i);

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
        const bool mainWrapped = phases[i] >= 1.f;
        if (mainWrapped) phases[i] -= std::floor(phases[i]);

        auxPhases[i] += auxPhaseIncs[i] * (params.model == Model::Sequential ? 1.002f : 1.f);
        if (params.model == Model::Sequential && mainWrapped)
            auxPhases[i] = 0.f;
        else if (auxPhases[i] >= 1.f)
            auxPhases[i] -= std::floor(auxPhases[i]);

        subPhases[i] += subPhaseIncs[i];
        if (subPhases[i] >= 1.f) subPhases[i] -= std::floor(subPhases[i]);

        L += sample * panWeights[i][0];
        R += sample * panWeights[i][1];
    }

    return { L * outputScale, R * outputScale };
}
