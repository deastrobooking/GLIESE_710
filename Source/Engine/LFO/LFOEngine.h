#pragma once
#include <JuceHeader.h>
#include <array>

class LFOEngine
{
public:
    static constexpr int kNumLFOs = 4;

    enum class Shape { Sine, Triangle, Saw, RevSaw, Square, SampleHold, SmoothSH };

    // Sync division: 0=Free, then 1/32 increments
    static constexpr int kNumSyncRates = 9;
    // Sync rate in beats: { Free, 1/32, 1/16, 1/8, 1/4, 1/2, 1, 2, 4 }
    static constexpr float kSyncBeats[kNumSyncRates] = {
        0.f, 1.f/32.f, 1.f/16.f, 1.f/8.f, 0.25f, 0.5f, 1.f, 2.f, 4.f
    };

    struct LFOParams
    {
        Shape shape   = Shape::Sine;
        float rate    = 1.f;     // Hz when free-running
        int   syncIdx = 0;       // index into kSyncBeats (0 = free)
        float amount  = 0.5f;   // output scale 0..1
        float phase   = 0.f;    // start phase offset 0..1
        bool  retrig  = false;
    };

    void prepare(double sampleRate);
    void setParams(int lfoIndex, const LFOParams& p);
    void setHostBPM(double bpm);

    void noteOn();
    void reset();

    // Process all 4 LFOs, returns pointer to array of kNumLFOs values
    void processBlock(int numSamples);

    // Get current value of LFO i (call after processBlock)
    float getValue(int i) const { return values[i]; }

private:
    double sr  = 48000.0;
    double bpm = 120.0;

    std::array<LFOParams, kNumLFOs> params;
    std::array<float, kNumLFOs>     phases  = {};
    std::array<float, kNumLFOs>     values  = {};
    std::array<float, kNumLFOs>     shPrev  = {};  // S&H hold value

    // LCG noise per LFO
    std::array<uint32_t, kNumLFOs>  seeds = { 1u, 2u, 3u, 4u };

    float generateShape(int i, float phase);
    float whiteSample(int i);
    float lfoRateHz(int i) const;
};
