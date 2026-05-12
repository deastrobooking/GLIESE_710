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

    // coarse 0 → 0.5x, coarse N → N×
    float ratio = (params.coarse == 0) ? 0.5f : (float)params.coarse;
    ratio = juce::jlimit(0.01f, 64.f, ratio + params.ratioOffset);
    ratio *= std::pow(2.f, params.fine / 12.f);
    return noteFreq * ratio;
}

float FMOperator::processSample(float modInput)
{
    float envGain = env.processSample();
    float hz      = getFrequency();
    float phaseInc = hz / (float)sr;

    // feedback is self-modulation (scaled by π for musical range)
    float fbAmt  = params.feedback * juce::MathConstants<float>::pi;
    float input  = modInput + fbAmt * prevOut;

    float out = std::sin(phase * juce::MathConstants<float>::twoPi + input);

    phase += phaseInc;
    if (phase >= 1.f) phase -= 1.f;

    prevOut = out;
    return out * params.level * envGain;
}
