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
    params.fine          = juce::jlimit(-1.f, 1.f, params.fine);
    params.detune        = juce::jlimit(-15.f, 15.f, params.detune);
    params.level         = juce::jlimit(0.f, 1.f, params.level);
    params.feedback      = juce::jlimit(0.f, 1.f, params.feedback);
    params.spectralSkirt = juce::jlimit(0.f, 1.f, params.spectralSkirt);
    params.spectralResonance = juce::jlimit(0.f, 1.f, params.spectralResonance);
    params.velocityDepth = juce::jlimit(0.f, 1.f, params.velocityDepth);
    params.keyScaleDepth = juce::jlimit(-1.f, 1.f, params.keyScaleDepth);
    params.phaseOffset   -= std::floor(params.phaseOffset);
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
    phase   = params.phaseOffset;
    prevOut = 0.f;
    env.reset();
}

void FMOperator::noteOn()
{
    if (params.keyOnReset)
    {
        phase = params.phaseOffset;
        prevOut = 0.f;
    }

    env.noteOn();
}

float FMOperator::getFrequency() const
{
    if (params.fixedFreq) return params.fixedHz;

    float ratio = (params.coarse == 0) ? 0.5f : (float)params.coarse;
    ratio = juce::jlimit(0.01f, 64.f, ratio + params.ratioOffset);
    ratio *= std::pow(2.f, params.fine / 12.f);
    ratio *= std::pow(2.f, params.detune / 1200.f);
    return noteFreq * ratio;
}

float FMOperator::generateSpectralWave(float p, bool oddOnly, bool narrow, bool resonant) const noexcept
{
    constexpr int kMaxPartials = 24;
    const int maxPartial = narrow ? 12 : kMaxPartials;
    const float rolloff = 0.25f + params.spectralSkirt * 2.75f;
    const float resCentre = 1.f + params.spectralResonance * (float)(maxPartial - 1);
    const float resWidth = narrow ? 1.35f : 2.25f;

    float sum = 0.f;
    float norm = 0.f;

    for (int h = 1; h <= maxPartial; ++h)
    {
        if (oddOnly && (h % 2 == 0))
            continue;

        float amp = 1.f / std::pow((float)h, rolloff);
        if (resonant)
        {
            float dist = ((float)h - resCentre) / resWidth;
            amp *= 0.25f + 1.75f * std::exp(-0.5f * dist * dist);
        }

        sum += amp * std::sin(juce::MathConstants<float>::twoPi * (float)h * p);
        norm += amp;
    }

    return norm > 1.0e-6f ? juce::jlimit(-1.f, 1.f, sum / norm * 2.2f) : 0.f;
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

        case Wave::AllWide:
            return generateSpectralWave(p, false, false, false);

        case Wave::AllNarrow:
            return generateSpectralWave(p, false, true, false);

        case Wave::OddWide:
            return generateSpectralWave(p, true, false, false);

        case Wave::OddNarrow:
            return generateSpectralWave(p, true, true, false);

        case Wave::ResWide:
            return generateSpectralWave(p, false, false, true);

        case Wave::ResNarrow:
            return generateSpectralWave(p, false, true, true);

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
