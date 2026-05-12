#include "ADSREnvelope.h"

static constexpr float kTargetRatioA  = 0.3f;
static constexpr float kTargetRatioDR = 0.0001f;

float ADSREnvelope::calcCoeff(float timeSec, double sampleRate)
{
    if (timeSec <= 0.f) return 0.f;
    return std::exp(-std::log((1.f + kTargetRatioDR) / kTargetRatioDR)
                    / (timeSec * (float)sampleRate));
}

void ADSREnvelope::recalcCoeffs()
{
    attackCoeff  = calcCoeff(params.attackSec,  sr);
    decayCoeff   = calcCoeff(params.decaySec,   sr);
    releaseCoeff = calcCoeff(params.releaseSec, sr);
}

void ADSREnvelope::prepare(double sampleRate)
{
    sr = sampleRate;
    recalcCoeffs();
    reset();
}

void ADSREnvelope::setParams(const Params& p)
{
    params = p;
    recalcCoeffs();
}

void ADSREnvelope::noteOn()
{
    stage = Stage::Attack;
}

void ADSREnvelope::noteOff()
{
    if (stage != Stage::Idle)
        stage = Stage::Release;
}

void ADSREnvelope::reset()
{
    stage = Stage::Idle;
    value = 0.f;
}

float ADSREnvelope::processSample()
{
    switch (stage)
    {
        case Stage::Idle:
            break;

        case Stage::Attack:
            value = attackCoeff * (value - (1.f + kTargetRatioA)) + (1.f + kTargetRatioA);
            if (value >= 1.f)
            {
                value = 1.f;
                stage = Stage::Decay;
            }
            break;

        case Stage::Decay:
            value = decayCoeff * (value - params.sustain) + params.sustain;
            if (value <= params.sustain + 1e-4f)
            {
                value = params.sustain;
                stage = Stage::Sustain;
            }
            break;

        case Stage::Sustain:
            value = params.sustain;
            break;

        case Stage::Release:
            value = releaseCoeff * (value - kTargetRatioDR) + kTargetRatioDR;
            if (value <= kTargetRatioDR + 1e-4f)
            {
                value = 0.f;
                stage = Stage::Idle;
            }
            break;
    }
    return value;
}
