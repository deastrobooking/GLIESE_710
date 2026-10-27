#pragma once
#include <JuceHeader.h>

class KarplusStrong
{
public:
    struct Params
    {
        float decay      = 0.996f;   // loop feedback [0.99..0.9999]
        float brightness = 0.5f;    // LPF blend: 1=bright/slow decay, 0=dark/fast decay
        float stiffness  = 0.0f;    // all-pass dispersion [-0.95..0.95]
        float morph      = 0.0f;    // excitation: 0=white noise, 1=glitch burst
        float position   = 0.3f;    // pluck position [0..1]
    };

    static constexpr int kMaxDelaySamples = 8192;

    void  prepare(double sampleRate);
    void  setFrequency(float hz);
    void  pluck(float velocity, const Params& params);
    float processSample(const Params& params);
    void  reset();
    bool  isActive() const { return active; }

private:
    float    delayBuffer[kMaxDelaySamples] = {};
    int      bufLen   = kMaxDelaySamples;
    int      writeIdx = 0;
    float    lastLPFOut = 0.0f;
    float    lastAPFIn  = 0.0f;
    float    lastAPFOut = 0.0f;
    float    frequency  = 440.0f;
    double   sr         = 48000.0;
    bool     active     = false;
    int      quietSamples = 0;
    uint32_t noiseSeed  = 12345u;

    float nextNoise();
    float readInterpolated(float delaySamples) const;
};
