#pragma once
#include <JuceHeader.h>
#include "FilterTypes.h"
#include "FilterModels.h"
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
        float      morph     = 0.5f;      // 0..1
        float      saturation = 0.35f;    // 0..1
        float      bassComp  = 0.f;       // 0..1
        float      outputGainDb = 0.f;    // dB
        float      mix       = 1.f;       // 0..1 wet
        int        oversample = 0;        // 0=off, 1=2x, 2=4x
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

    // IIR for shelf/peak (notch built from HP+LP sum)
    juce::dsp::IIR::Filter<float> iirFilter;

    CleanSVF cleanSVF;
    LadderLP ladderLP;
    SEMMorph semMorph;
    MSDual msDual;
    CombFilter combFilter;
    FormantFilter formantFilter;

    float lastConfiguredCutoff = -1.f;
    float lastConfiguredRes = -1.f;
    float lastConfiguredDrive = -1.f;
    FilterMode lastConfiguredMode = FilterMode::NumModes;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    juce::SmoothedValue<float> resonanceSmooth;
    juce::SmoothedValue<float> driveSmooth;
    juce::SmoothedValue<float> morphSmooth;
    juce::SmoothedValue<float> mixSmooth;
    bool smoothingReady = false;

    float computeCutoff(float baseCutoff, float envValue, float lfoValue) const;
    bool  needsReconfigure(float cutHz, float res, float drive) const;
    void  markConfigured(float cutHz, float res, float drive);
    void  configureIIR(float cutHz, float res, float drive);
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
