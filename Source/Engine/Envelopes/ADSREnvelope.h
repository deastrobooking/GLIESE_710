#pragma once
#include <JuceHeader.h>

class ADSREnvelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };
    enum class Curve  { Linear, Exponential };

    struct Params
    {
        float attackSec  = 0.01f;
        float decaySec   = 0.2f;
        float sustain    = 0.8f;   // 0..1
        float releaseSec = 0.3f;
        Curve curve      = Curve::Exponential;
    };

    void prepare(double sampleRate);
    void setParams(const Params& p);

    void noteOn();
    void noteOff();
    void reset();

    float processSample();
    bool  isActive() const { return stage != Stage::Idle; }
    Stage getStage() const { return stage; }
    float getCurrentValue() const { return value; }

private:
    double sr        = 48000.0;
    float  value     = 0.f;
    Stage  stage     = Stage::Idle;
    Params params;

    float attackCoeff  = 0.f;
    float decayCoeff   = 0.f;
    float releaseCoeff = 0.f;

    void recalcCoeffs();
    static float calcCoeff(float timeSec, double sampleRate);
};
