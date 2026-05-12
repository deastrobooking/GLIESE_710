#pragma once
#include <JuceHeader.h>
#include "../Envelopes/ADSREnvelope.h"

class FMOperator
{
public:
    struct Params
    {
        int   coarse   = 1;       // frequency ratio integer part (0 = 0.5)
        float fine     = 0.f;     // ±1 semitone range
        float ratioOffset = 0.f;  // additive ratio modulation
        float level    = 1.f;     // output level / modulation depth 0..1
        float feedback = 0.f;     // self-feedback 0..1
        bool  fixedFreq = false;  // if true, uses fixedHz instead of ratio*noteFreq
        float fixedHz   = 440.f;
    };

    void prepare(double sampleRate);
    void setParams(const Params& p);
    void setNoteFrequency(float noteHz);

    // Process one sample. modInput = summed modulation in radians.
    // Returns operator output (before envelope scaling for modulators,
    // after for carriers).
    float processSample(float modInput);

    void noteOn()  { env.noteOn(); }
    void noteOff() { env.noteOff(); }
    void reset();

    float getEnvValue() const { return env.getCurrentValue(); }
    bool  isActive()    const { return env.isActive(); }

    ADSREnvelope& getEnvelope() { return env; }

private:
    double sr       = 48000.0;
    float  phase    = 0.f;
    float  prevOut  = 0.f;   // for feedback
    float  noteFreq = 440.f;
    Params params;
    ADSREnvelope env;

    float getFrequency() const;
};
