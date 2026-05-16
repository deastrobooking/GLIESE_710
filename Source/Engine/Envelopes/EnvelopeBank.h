#pragma once
#include "ADSREnvelope.h"

// Indices into the envelope array
enum EnvIndex
{
    ENV_MASTER = 0,
    ENV_3D,
    ENV_FM,
    ENV_WT,
    ENV_VA,
    ENV_VA2,
    ENV_VA3,
    ENV_VA4,
    ENV_KS,
    ENV_GRAN,
    ENV_FILTER_A,
    ENV_FILTER_B,
    ENV_COUNT
};

class EnvelopeBank
{
public:
    void prepare(double sampleRate);
    void noteOn();
    void noteOff();
    void reset();

    ADSREnvelope& get(int index) { return envs[index]; }
    float processAndGet(int index) { return envs[index].processSample(); }

    bool anyActive() const;

private:
    ADSREnvelope envs[ENV_COUNT];
};
