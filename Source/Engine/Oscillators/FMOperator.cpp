#include "FMOperator.h"

void FMOperator::prepare(double sampleRate)
{
    sr = sampleRate;
    env.prepare(sampleRate);
    reset();
}

void FMOperator::setParams(const Params& p)
{
    params = p;
}

void FMOperator::setNoteFrequency(float noteHz)
{
    noteFreq = noteHz;
}

void FMOperator::reset()
{
    phase   = 0.f;
    prevOut = 0.f;
    env.reset();
}

float FMOperator::getFrequency() const
{
    if (params.fixedFreq) return params.fixedHz;

    float ratio = (params.coarse == 0) ? 0.5f : (float)params.coarse;
    ratio = juce::jlimit(0.01f, 64.f, ratio + params.ratioOffset);
    ratio *= std::pow(2.f, params.fine / 12.f);
    return noteFreq * ratio;
}

float FMOperator::generateWave(float p) noexcept
{
    // p is normalized phase in [0, 1)
    switch (params.waveShape)
    {
        case Wave::Triangle:
            return 1.f - 4.f * std::abs(p - 0.5f);

        case Wave::Saw:
            return 2.f * p - 1.f;

        case Wave::Square:
            return p < 0.5f ? 1.f : -1.f;

        case Wave::Pulse:
            return p < 0.25f ? 1.f : -1.f;

        case Wave::Noise:
            return rng.nextFloat() * 2.f - 1.f;

        case Wave::Sine:
        default:
            return std::sin(p * juce::MathConstants<float>::twoPi);
    }
}

float FMOperator::processSample(float modInput)
{
    float envGain = env.processSample();
    float hz      = getFrequency();
    float phaseInc = hz / (float)sr;

    float fbAmt = params.feedback * juce::MathConstants<float>::pi;
    float totalPhase = phase + (modInput + fbAmt * prevOut) / juce::MathConstants<float>::twoPi;

    // Wrap to [0, 1)
    totalPhase -= std::floor(totalPhase);

    float out = generateWave(totalPhase);

    phase += phaseInc;
    if (phase >= 1.f) phase -= 1.f;

    prevOut = out;

    // Velocity scaling: depth=0 means level is velocity-independent
    float velMult = 1.f - params.velocityDepth * (1.f - velocity);

    // Key scaling: depth > 0 means level decreases away from C4, < 0 means increases
    float octavesFromC4 = (float)(midiNote - 60) / 12.f;
    float keyMult = juce::jlimit(0.f, 2.f, 1.f - params.keyScaleDepth * octavesFromC4);

    return out * params.level * envGain * velMult * keyMult;
}
