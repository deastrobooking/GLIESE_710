#pragma once

#include <JuceHeader.h>
#include "FilterTypes.h"
#include <array>

struct FilterProcessParams
{
    FilterMode mode = FilterMode::LP12;
    float cutoff = 20000.f;
    float resonance = 0.f;
    float drive = 0.f;
    float morph = 0.5f;
    float saturation = 0.35f;
    float bassComp = 0.f;
    float outputGainDb = 0.f;
    float mix = 1.f;
    int oversample = 0; // 0=off, 1=2x, 2=4x
};

class IFilter
{
public:
    virtual ~IFilter() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void reset() = 0;
    virtual float processSample(float input, const FilterProcessParams& params) = 0;
};

namespace FilterModelDetail
{
    inline float saturate(float x, float amount)
    {
        amount = juce::jlimit(0.f, 1.f, amount);
        if (amount <= 0.0001f)
            return x;

        const float gain = 1.f + amount * 5.f;
        return std::tanh(x * gain) / std::tanh(gain);
    }

    inline int oversampleFactor(int setting)
    {
        return setting == 2 ? 4 : (setting == 1 ? 2 : 1);
    }

    inline float clampAudio(float x)
    {
        return std::isfinite(x) ? juce::jlimit(-8.f, 8.f, x) : 0.f;
    }

    inline float lerp(float a, float b, float t)
    {
        return a + (b - a) * juce::jlimit(0.f, 1.f, t);
    }
}

class CleanSVF final : public IFilter
{
public:
    void prepare(double sampleRate) override
    {
        juce::dsp::ProcessSpec spec { sampleRate, 512, 1 };
        svf1.prepare(spec);
        svf2.prepare(spec);
        reset();
    }

    void reset() override
    {
        svf1.reset();
        svf2.reset();
        lastMode = FilterMode::NumModes;
        lastCutoff = -1.f;
        lastResonance = -1.f;
    }

    float processSample(float input, const FilterProcessParams& params) override
    {
        configure(params);

        switch (params.mode)
        {
            case FilterMode::LP12:
            case FilterMode::HP12:
            case FilterMode::Bandpass:
                return svf1.processSample(0, input);

            case FilterMode::LP24:
            case FilterMode::HP24:
            {
                const float mid = svf1.processSample(0, input);
                return svf2.processSample(0, mid);
            }

            case FilterMode::Notch:
            {
                using Type = juce::dsp::StateVariableTPTFilterType;
                svf1.setType(Type::lowpass);
                svf2.setType(Type::highpass);
                const float lp = svf1.processSample(0, input);
                const float hp = svf2.processSample(0, input);
                return (lp + hp) * 0.5f;
            }

            case FilterMode::Peak:
            case FilterMode::LowShelf:
            case FilterMode::HighShelf:
            case FilterMode::Comb:
            case FilterMode::AM_Plus:
            case FilterMode::AM_Minus:
            case FilterMode::Ladder12:
            case FilterMode::Ladder18:
            case FilterMode::Ladder24:
            case FilterMode::SEMMorph:
            case FilterMode::MSDual:
            case FilterMode::Formant:
            case FilterMode::NumModes:
                return input;
        }

        return input;
    }

private:
    juce::dsp::StateVariableTPTFilter<float> svf1;
    juce::dsp::StateVariableTPTFilter<float> svf2;
    FilterMode lastMode = FilterMode::NumModes;
    float lastCutoff = -1.f;
    float lastResonance = -1.f;

    void configure(const FilterProcessParams& params)
    {
        if (params.mode == lastMode
            && std::abs(params.cutoff - lastCutoff) < 0.5f
            && std::abs(params.resonance - lastResonance) < 0.0001f)
            return;

        using Type = juce::dsp::StateVariableTPTFilterType;
        const float q = juce::jlimit(0.5f, 20.f, 0.5f + params.resonance * 19.5f);

        svf1.setCutoffFrequency(params.cutoff);
        svf1.setResonance(q);
        svf2.setCutoffFrequency(params.cutoff);
        svf2.setResonance(q);

        switch (params.mode)
        {
            case FilterMode::LP12:
            case FilterMode::LP24:
                svf1.setType(Type::lowpass);
                svf2.setType(Type::lowpass);
                break;
            case FilterMode::HP12:
            case FilterMode::HP24:
                svf1.setType(Type::highpass);
                svf2.setType(Type::highpass);
                break;
            case FilterMode::Bandpass:
                svf1.setType(Type::bandpass);
                break;
            case FilterMode::Notch:
                svf1.setType(Type::lowpass);
                svf2.setType(Type::highpass);
                break;
            case FilterMode::Peak:
            case FilterMode::LowShelf:
            case FilterMode::HighShelf:
            case FilterMode::Comb:
            case FilterMode::AM_Plus:
            case FilterMode::AM_Minus:
            case FilterMode::Ladder12:
            case FilterMode::Ladder18:
            case FilterMode::Ladder24:
            case FilterMode::SEMMorph:
            case FilterMode::MSDual:
            case FilterMode::Formant:
            case FilterMode::NumModes:
                break;
        }

        lastMode = params.mode;
        lastCutoff = params.cutoff;
        lastResonance = params.resonance;
    }
};

class LadderLP final : public IFilter
{
public:
    void prepare(double sampleRate) override
    {
        sr = sampleRate;
        reset();
    }

    void reset() override
    {
        z.fill(0.f);
    }

    float processSample(float input, const FilterProcessParams& params) override
    {
        const int factor = FilterModelDetail::oversampleFactor(params.oversample);
        const double coreSr = sr * (double) factor;
        float out = 0.f;

        for (int i = 0; i < factor; ++i)
            out = processCore(input, params, coreSr);

        return out;
    }

private:
    double sr = 48000.0;
    std::array<float, 4> z {};

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const int poles = params.mode == FilterMode::Ladder12 ? 2
                        : params.mode == FilterMode::Ladder18 ? 3 : 4;
        const float cutoff = juce::jlimit(20.f, (float) coreSr * 0.43f, params.cutoff);
        const float g = std::tan(juce::MathConstants<float>::pi * cutoff / (float) coreSr);
        const float G = g / (1.f + g);
        const float feedback = params.resonance * 3.85f;

        float stageIn = input * (1.f + params.drive * 8.f) - feedback * z[(size_t) poles - 1];
        stageIn = FilterModelDetail::saturate(stageIn, params.saturation);

        for (int i = 0; i < poles; ++i)
        {
            const float v = (stageIn - z[(size_t) i]) * G;
            const float y = v + z[(size_t) i];
            z[(size_t) i] = y + v;
            stageIn = FilterModelDetail::saturate(y, params.saturation * 0.45f);
        }

        const float comp = 1.f + params.bassComp * params.resonance * 0.85f;
        return FilterModelDetail::clampAudio(z[(size_t) poles - 1] * comp);
    }
};

class SEMMorph final : public IFilter
{
public:
    void prepare(double sampleRate) override
    {
        sr = sampleRate;
        reset();
    }

    void reset() override
    {
        low = 0.f;
        band = 0.f;
    }

    float processSample(float input, const FilterProcessParams& params) override
    {
        const int factor = FilterModelDetail::oversampleFactor(params.oversample);
        const double coreSr = sr * (double) factor;
        float out = 0.f;

        for (int i = 0; i < factor; ++i)
            out = processCore(input, params, coreSr);

        return out;
    }

private:
    double sr = 48000.0;
    float low = 0.f;
    float band = 0.f;

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const float cutoff = juce::jlimit(20.f, (float) coreSr * 0.22f, params.cutoff);
        const float f = juce::jlimit(0.001f, 0.99f,
            2.f * std::sin(juce::MathConstants<float>::pi * cutoff / (float) coreSr));
        const float damping = juce::jlimit(0.05f, 2.f, 1.75f - params.resonance * 1.65f);

        const float x = FilterModelDetail::saturate(input * (1.f + params.drive * 5.f),
                                                    params.saturation);
        const float high = x - low - damping * band;
        band += f * high;
        low += f * band;

        const float notch = (low + high) * 0.5f;
        const float m = juce::jlimit(0.f, 1.f, params.morph);
        const float out = m < 0.5f
            ? FilterModelDetail::lerp(low, notch, m * 2.f)
            : FilterModelDetail::lerp(notch, high, (m - 0.5f) * 2.f);

        return FilterModelDetail::clampAudio(FilterModelDetail::saturate(out, params.saturation * 0.35f));
    }
};

class MSDual final : public IFilter
{
public:
    void prepare(double sampleRate) override
    {
        sr = sampleRate;
        reset();
    }

    void reset() override
    {
        hpLow = 0.f;
        lp1 = 0.f;
        lp2 = 0.f;
    }

    float processSample(float input, const FilterProcessParams& params) override
    {
        const int factor = FilterModelDetail::oversampleFactor(params.oversample);
        const double coreSr = sr * (double) factor;
        float out = 0.f;

        for (int i = 0; i < factor; ++i)
            out = processCore(input, params, coreSr);

        return out;
    }

private:
    double sr = 48000.0;
    float hpLow = 0.f;
    float lp1 = 0.f;
    float lp2 = 0.f;

    static float onePoleG(float cutoff, double sampleRate)
    {
        return juce::jlimit(0.0001f, 0.99f,
            1.f - std::exp(-2.f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate));
    }

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const float cutoff = juce::jlimit(20.f, (float) coreSr * 0.35f, params.cutoff);
        const float hpCut = juce::jlimit(20.f, (float) coreSr * 0.20f,
                                         cutoff * (0.15f + params.morph * 0.6f));
        const float gHp = onePoleG(hpCut, coreSr);
        const float gLp = onePoleG(cutoff, coreSr);

        const float x = FilterModelDetail::saturate(input * (1.f + params.drive * 9.f),
                                                    params.saturation);
        hpLow += gHp * (x - hpLow);
        const float hp = x - hpLow;

        const float fb = params.resonance * 2.4f * lp2;
        const float driven = FilterModelDetail::saturate(hp - fb, params.saturation);
        lp1 += gLp * (driven - lp1);
        lp1 = FilterModelDetail::saturate(lp1, params.saturation * 0.4f);
        lp2 += gLp * (lp1 - lp2);

        const float bp = lp1 - lp2;
        const float m = juce::jlimit(0.f, 1.f, params.morph);
        const float out = m < 0.5f
            ? FilterModelDetail::lerp(lp2, bp, m * 2.f)
            : FilterModelDetail::lerp(bp, hp, (m - 0.5f) * 2.f);

        return FilterModelDetail::clampAudio(out * (1.f + params.bassComp * params.resonance * 0.4f));
    }
};

class CombFilter final : public IFilter
{
public:
    void prepare(double sampleRate) override
    {
        sr = sampleRate;
        reset();
    }

    void reset() override
    {
        buffer.fill(0.f);
        writePos = 0;
    }

    float processSample(float input, const FilterProcessParams& params) override
    {
        const int delaySamples = juce::jlimit(1, kMaxDelaySamples - 1,
            (int) (sr / juce::jlimit(20.f, (float) sr * 0.49f, params.cutoff)));
        const int readPos = (writePos - delaySamples + kMaxDelaySamples) % kMaxDelaySamples;
        const float delayed = buffer[(size_t) readPos];
        const float fb = juce::jlimit(0.f, 0.97f, 0.25f + params.resonance * 0.72f);
        const float driven = FilterModelDetail::saturate(input * (1.f + params.drive * 4.f),
                                                         params.saturation);
        const float out = FilterModelDetail::clampAudio(driven + delayed * fb);
        buffer[(size_t) writePos] = out;
        writePos = (writePos + 1) % kMaxDelaySamples;
        return out;
    }

private:
    static constexpr int kMaxDelaySamples = 4096;
    double sr = 48000.0;
    std::array<float, kMaxDelaySamples> buffer {};
    int writePos = 0;
};

class FormantFilter final : public IFilter
{
public:
    void prepare(double sampleRate) override
    {
        sr = sampleRate;
        reset();
    }

    void reset() override
    {
        for (auto& b : bands)
            b.reset();
    }

    float processSample(float input, const FilterProcessParams& params) override
    {
        static constexpr float vowels[5][3] {
            { 800.f, 1150.f, 2900.f }, // A
            { 400.f, 1700.f, 2600.f }, // E
            { 350.f, 2000.f, 2800.f }, // I
            { 450.f,  800.f, 2830.f }, // O
            { 325.f,  700.f, 2530.f }  // U
        };
        static constexpr float gains[3] { 1.0f, 0.62f, 0.35f };

        const float pos = juce::jlimit(0.f, 1.f, params.morph) * 4.f;
        const int i0 = juce::jlimit(0, 4, (int) std::floor(pos));
        const int i1 = juce::jlimit(0, 4, i0 + 1);
        const float t = pos - (float) i0;
        const float scale = juce::jlimit(0.45f, 2.5f, params.cutoff / 1000.f);
        const float q = 4.f + params.resonance * 28.f;
        const float driven = FilterModelDetail::saturate(input * (1.f + params.drive * 5.f),
                                                         params.saturation);

        float out = 0.f;
        for (int i = 0; i < 3; ++i)
        {
            const float f = FilterModelDetail::lerp(vowels[i0][i], vowels[i1][i], t) * scale;
            bands[(size_t) i].setBandpass(sr, juce::jlimit(40.f, (float) sr * 0.45f, f), q);
            out += bands[(size_t) i].process(driven) * gains[i];
        }

        return FilterModelDetail::clampAudio(out * 0.45f);
    }

private:
    struct Biquad
    {
        float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
        float z1 = 0.f, z2 = 0.f;

        void reset()
        {
            z1 = 0.f;
            z2 = 0.f;
        }

        void setBandpass(double sampleRate, float freq, float q)
        {
            const float omega = 2.f * juce::MathConstants<float>::pi * freq / (float) sampleRate;
            const float sn = std::sin(omega);
            const float cs = std::cos(omega);
            const float alpha = sn / (2.f * juce::jmax(0.1f, q));
            const float a0 = 1.f + alpha;
            b0 = alpha / a0;
            b1 = 0.f;
            b2 = -alpha / a0;
            a1 = (-2.f * cs) / a0;
            a2 = (1.f - alpha) / a0;
        }

        float process(float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return FilterModelDetail::clampAudio(y);
        }
    };

    double sr = 48000.0;
    std::array<Biquad, 3> bands {};
};
