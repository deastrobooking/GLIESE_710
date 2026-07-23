#pragma once
#include <JuceHeader.h>
#include <array>

class LFOEngine
{
public:
    static constexpr int kNumLFOs = 4;

    enum class Shape { Sine, Triangle, Saw, RevSaw, Square, SampleHold, SmoothSH };

    // Sync rate index: 0 = 8 bars, 10 = 1/128 note.
    static constexpr int kNumSyncRates = 11;
    static constexpr float kSyncBeats[kNumSyncRates] = {
        32.f, 16.f, 8.f, 4.f, 2.f, 1.f, 0.5f, 0.25f, 0.125f, 0.0625f, 0.03125f
    };

    struct LFOParams
    {
        Shape shape   = Shape::Sine;
        float rate    = 1.f;     // Hz when free-running
        bool  sync    = false;
        float syncRate = 5.f;    // 0..10, continuous between kSyncBeats entries
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
    void processSample();

    // Get current value of LFO i (call after processBlock)
    float getValue(int i) const { return values[(size_t) juce::jlimit(0, kNumLFOs - 1, i)]; }

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
