#pragma once
#include "ADSREnvelope.h"

// Indices into the envelope array
enum EnvIndex
{
    ENV_MASTER = 0,
    ENV_FM,
    ENV_FM2,
    ENV_3D1,
    ENV_3D2,
    ENV_3D3,
    ENV_3D4,
    ENV_3D5,
    ENV_3D6,
    ENV_3D7,
    ENV_3D8,
    ENV_WT1,
    ENV_WT2,
    ENV_WT3,
    ENV_WT4,
    ENV_WT5,
    ENV_WT6,
    ENV_WT7,
    ENV_WT8,
    ENV_VA1,
    ENV_VA2,
    ENV_VA3,
    ENV_VA4,
    ENV_VA5,
    ENV_VA6,
    ENV_VA7,
    ENV_VA8,
    ENV_KS1,
    ENV_KS2,
    ENV_KS3,
    ENV_KS4,
    ENV_KS5,
    ENV_KS6,
    ENV_KS7,
    ENV_KS8,
    ENV_GRAN1,
    ENV_GRAN2,
    ENV_GRAN3,
    ENV_GRAN4,
    ENV_GRAN5,
    ENV_GRAN6,
    ENV_GRAN7,
    ENV_GRAN8,
    ENV_TERRAIN1,
    ENV_TERRAIN2,
    ENV_TERRAIN3,
    ENV_TERRAIN4,
    ENV_TERRAIN5,
    ENV_TERRAIN6,
    ENV_TERRAIN7,
    ENV_TERRAIN8,
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
