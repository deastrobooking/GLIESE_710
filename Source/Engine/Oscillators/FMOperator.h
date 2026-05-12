#pragma once
#include <JuceHeader.h>
#include "../Envelopes/ADSREnvelope.h"

class FMOperator
{
public:
    enum class Wave { Sine = 0, Triangle, Saw, Square, Pulse, Noise, NumWaves };

    struct Params
    {
        int   coarse   = 1;       // frequency ratio integer part (0 = 0.5)
        float fine     = 0.f;     // ±1 semitone range
        float ratioOffset = 0.f;  // additive ratio modulation
        float level    = 1.f;     // output level / modulation depth 0..1
        float feedback = 0.f;     // self-feedback 0..1
        bool  fixedFreq = false;  // if true, uses fixedHz instead of ratio*noteFreq
        float fixedHz   = 440.f;

        Wave  waveShape     = Wave::Sine;  // operator waveform
        float velocityDepth = 0.f;         // 0 = velocity ignored, 1 = linear vel scaling
        float keyScaleDepth = 0.f;         // level change per octave from C4 (-1..+1)
    };

    void prepare(double sampleRate);
    void setParams(const Params& p);
    void setNoteFrequency(float noteHz);
    void setVelocity(float v)  { velocity  = juce::jlimit(0.f, 1.f, v); }
    void setMidiNote(int note) { midiNote  = note; }

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
    float  prevOut  = 0.f;
    float  noteFreq = 440.f;
    float  velocity = 1.f;
    int    midiNote = 60;
    Params params;
    ADSREnvelope env;

    juce::Random rng;

    float getFrequency() const;
    float generateWave(float normalizedPhase) noexcept;
};
