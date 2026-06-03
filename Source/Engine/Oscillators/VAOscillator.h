#pragma once
#include <JuceHeader.h>

class VAOscillator
{
public:
    enum class Model { Native, Moog, Roland, Oberheim, Sequential, NumModels };
    enum class Shape { Sine, Saw, RevSaw, Square, Triangle, Pulse, NumShapes };

    struct Params
    {
        Model model      = Model::Native;
        Shape shape      = Shape::Saw;
        float shapeMorph = -1.f;   // 0..5, continuous crossfade between shapes
        float pulseWidth = 0.5f;   // 0..1 (square/pulse only)
        float detune     = 0.0f;   // semitones per unison voice spread
        int   unisonCount = 1;     // 1..8
        float spread     = 0.5f;   // stereo spread 0..1
        float ringMod    = 0.0f;   // 0..1 internal ring modulation
        float wavefold   = 0.0f;   // 0..1 wavefold drive/mix
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
    float auxPhases[kMaxUnison] = {};
    float auxPhaseIncs[kMaxUnison] = {};
    float subPhases[kMaxUnison] = {};
    float subPhaseIncs[kMaxUnison] = {};
    float driftPhases[kMaxUnison] = {};
    float hpLastIn[kMaxUnison] = {};
    float hpLastOut[kMaxUnison] = {};
    float panWeights[kMaxUnison][2] = {};  // [voice][L/R]
    float outputScale = 1.f;

    void rebuildVoiceState();

    // PolyBLEP helpers
    static float polyBlep(float phase, float phaseInc);
    float generateSaw(float phase, float phaseInc) const;
    float generateSquare(float phase, float phaseInc) const;
    float generatePulse(float phase, float phaseInc, float pulseWidth) const;
    float generateTriangle(float phase) const;
    float generateMorphedShape(float phase, float phaseInc) const;
    float generateShapeAt(float shapeMorph, float pulseWidth, float phase, float phaseInc) const;
    float applyModelShape(float x, int voiceIndex) noexcept;
    float highPassLean(float x, int voiceIndex) noexcept;
    float modelDriftRatio(int voiceIndex) noexcept;
    float modelCalibrationCents(int voiceIndex) const noexcept;
    static float softClip(float x, float drive) noexcept;
    static float asymmetry(float x, float amount) noexcept;
    static float centsToRatio(float cents) noexcept;
    static float foldSignal(float x) noexcept;

    // Per-unison voice triangle integrator state
    float triIntegrator[kMaxUnison] = {};
};
