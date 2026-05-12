#include "FMOperator.h"

void FMOperator::prepare(double sampleRate)
{
    sr = sampleRate;
    env.prepare(sampleRate);
    updateDerived();
    reset();
}

void FMOperator::setParams(const Params& p)
{
    params = p;
    params.coarse        = juce::jlimit(0, 31, params.coarse);
    params.level         = juce::jlimit(0.f, 1.f, params.level);
    params.feedback      = juce::jlimit(0.f, 1.f, params.feedback);
    params.velocityDepth = juce::jlimit(0.f, 1.f, params.velocityDepth);
    params.keyScaleDepth = juce::jlimit(-1.f, 1.f, params.keyScaleDepth);
    params.waveShape     = static_cast<Wave>(juce::jlimit(0, (int)Wave::NumWaves - 1,
                                                          (int)params.waveShape));
    updateDerived();
}

void FMOperator::setNoteFrequency(float noteHz)
{
    noteFreq = noteHz;
    updateDerived();
}

void FMOperator::updateDerived()
{
    phaseInc = getFrequency() / (float)sr;

    float velMult = 1.f - params.velocityDepth * (1.f - velocity);
    float octavesFromC4 = (float)(midiNote - 60) / 12.f;
    float keyMult = juce::jlimit(0.f, 2.f, 1.f - params.keyScaleDepth * octavesFromC4);
    outputGain = params.level * velMult * keyMult;
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

        case Wave::NumWaves:
        case Wave::Sine:
        default:
            return std::sin(p * juce::MathConstants<float>::twoPi);
    }
}

float FMOperator::processSample(float modInput)
{
    float envGain = env.processSample();

    float fbAmt = params.feedback * juce::MathConstants<float>::pi;
    float totalPhase = phase + (modInput + fbAmt * prevOut) / juce::MathConstants<float>::twoPi;

    // Wrap to [0, 1)
    totalPhase -= std::floor(totalPhase);

    float out = generateWave(totalPhase);

    phase += phaseInc;
    phase -= std::floor(phase);

    prevOut = out;

    return out * envGain * outputGain;
}
