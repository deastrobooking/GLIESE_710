#pragma once
#include <JuceHeader.h>
#include "FilterTypes.h"
#include "../Envelopes/ADSREnvelope.h"
#include <array>

class SingleFilter
{
public:
    struct Params
    {
        FilterMode mode      = FilterMode::LP12;
        float      cutoff    = 20000.f;   // Hz
        float      resonance = 0.f;       // 0..1
        float      drive     = 0.f;       // 0..1
        float      envAmount = 0.f;       // -1..1
        float      lfoAmount = 0.f;       // -1..1
    };

    void prepare(double sampleRate);
    void setParams(const Params& p);

    // modulation: env value (0..1) and lfo value (-1..1)
    float processSample(float input, float envValue, float lfoValue, float amMod = 0.f);
    void  reset();

private:
    double sr = 48000.0;
    Params params;

    // Primary SVF (12 dB)
    juce::dsp::StateVariableTPTFilter<float> svf1;
    // Second SVF for 24 dB cascade
    juce::dsp::StateVariableTPTFilter<float> svf2;

    // IIR for shelf/peak (notch built from HP+LP sum)
    juce::dsp::IIR::Filter<float> iirFilter;

    // Comb filter delay line
    static constexpr int kMaxCombDelaySamples = 4096;
    std::array<float, kMaxCombDelaySamples> combBuffer{};
    int  combWritePos = 0;
    float combFeedback = 0.7f;

    // Drive waveshaper
    juce::dsp::WaveShaper<float> shaper;

    float computeCutoff(float envValue, float lfoValue) const;
    void  configureSVF(float cutHz, float res);
    void  configureIIR(float cutHz, float res);
};

//==============================================================================
class DualFilter
{
public:
    void prepare(double sampleRate);

    void setFilterA(const SingleFilter::Params& p);
    void setFilterB(const SingleFilter::Params& p);
    void setRouting(FilterRouting r);

    ADSREnvelope& getEnvA() { return envA; }
    ADSREnvelope& getEnvB() { return envB; }

    void noteOn();
    void noteOff();
    void reset();

    // lfoA/lfoB: -1..1 LFO values. amMod: AM modulator signal.
    std::pair<float, float> processSample(float inL, float inR,
                                          float lfoA, float lfoB,
                                          float amMod = 0.f);

private:
    double        sr = 48000.0;
    std::array<SingleFilter, 2> filterA, filterB;
    FilterRouting routing = FilterRouting::Series;
    ADSREnvelope  envA, envB;
};
