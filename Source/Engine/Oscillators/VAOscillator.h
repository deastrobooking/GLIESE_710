#pragma once
#include <JuceHeader.h>

class VAOscillator
{
public:
    enum class Shape { Sine, Saw, RevSaw, Square, Triangle, Pulse };

    struct Params
    {
        Shape shape      = Shape::Saw;
        float pulseWidth = 0.5f;   // 0..1 (square/pulse only)
        float detune     = 0.0f;   // semitones per unison voice spread
        int   unisonCount = 1;     // 1..8
        float spread     = 0.5f;   // stereo spread 0..1
    };

    void prepare(double sampleRate);
    void setParams(const Params& p);
    void setFrequency(float hz);

    // Returns summed stereo pair [L, R]
    std::pair<float, float> processSample();

    void reset();

private:
    static constexpr int kMaxUnison = 8;

    double sr    = 48000.0;
    float  freq  = 440.f;
    Params params;

    float phases[kMaxUnison] = {};
    float phaseIncs[kMaxUnison] = {};
    float panWeights[kMaxUnison][2] = {};  // [voice][L/R]
    float outputScale = 1.f;

    void rebuildVoiceState();

    // PolyBLEP helpers
    static float polyBlep(float phase, float phaseInc);
    float generateSaw(float& phase, float phaseInc) const;
    float generateSquare(float& phase, float phaseInc) const;
    float generateTriangle(float phase) const;

    // Per-unison voice triangle integrator state
    float triIntegrator[kMaxUnison] = {};
};
