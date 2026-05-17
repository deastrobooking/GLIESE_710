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
            case FilterMode::RolandJuno106:
            case FilterMode::RolandJuno60:
            case FilterMode::RolandJupiter4:
            case FilterMode::RolandJupiter8:
            case FilterMode::TB303:
            case FilterMode::SequentialP5Rev2:
            case FilterMode::SequentialP5Rev3:
            case FilterMode::SequentialP600:
            case FilterMode::SequentialP6:
            case FilterMode::SequentialProOne:
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
            case FilterMode::RolandJuno106:
            case FilterMode::RolandJuno60:
            case FilterMode::RolandJupiter4:
            case FilterMode::RolandJupiter8:
            case FilterMode::TB303:
            case FilterMode::SequentialP5Rev2:
            case FilterMode::SequentialP5Rev3:
            case FilterMode::SequentialP600:
            case FilterMode::SequentialP6:
            case FilterMode::SequentialProOne:
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

class TB303LP final : public IFilter
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
        feedbackLow = 0.f;
        cutoffMemory = 900.f;
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
    std::array<float, 3> z {};
    float feedbackLow = 0.f;
    float cutoffMemory = 900.f;

    static float softLimit(float x, float amount)
    {
        amount = juce::jlimit(0.f, 1.f, amount);
        if (amount <= 0.0001f)
            return x;

        const float drive = 1.f + amount * 4.5f;
        return std::tanh(x * drive) / drive;
    }

    static float asymLimit(float x, float amount)
    {
        const float bias = amount * 0.035f;
        return softLimit(x + bias, amount) - bias * 0.55f;
    }

    static float logNormaliseCutoff(float cutoff)
    {
        const float hz = juce::jlimit(20.f, 20000.f, cutoff);
        return juce::jlimit(0.f, 1.f, std::log(hz / 20.f) / std::log(1000.f));
    }

    static float onePoleG(float cutoff, double sampleRate)
    {
        const float g = std::tan(juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
        return g / (1.f + g);
    }

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const float accent = juce::jlimit(0.f, 1.f, params.morph);
        const float cutoffNorm = logNormaliseCutoff(params.cutoff);
        const float acidRangeHz = 55.f * std::pow(210.f, cutoffNorm);
        const float targetCutoff = juce::jlimit(25.f,
                                                juce::jmin(15500.f, (float) coreSr * 0.36f),
                                                acidRangeHz * std::pow(2.f, accent * 0.85f));
        cutoffMemory += (targetCutoff - cutoffMemory) * (0.18f + accent * 0.45f);

        const float cutoff = juce::jlimit(25.f,
                                          juce::jmin(15500.f, (float) coreSr * 0.36f),
                                          cutoffMemory);
        const float G = onePoleG(cutoff, coreSr);
        const float brightG = onePoleG(juce::jmin(cutoff * 2.05f, (float) coreSr * 0.42f), coreSr);

        const float hpHz = 140.f + accent * 520.f;
        const float hpG = 1.f - std::exp(-2.f * juce::MathConstants<float>::pi * hpHz / (float) coreSr);
        feedbackLow += hpG * (z[2] - feedbackLow);
        const float feedbackHp = z[2] - feedbackLow;

        const float res = juce::jlimit(0.f, 1.f, params.resonance);
        const float resCurve = std::pow(res, 0.58f);
        const float resAmount = resCurve * (3.25f + accent * 1.65f);
        const float sat = juce::jlimit(0.f, 1.f, params.saturation);
        const float driveGain = 0.85f + params.drive * 5.25f + accent * 1.35f;

        float stageIn = input * driveGain - softLimit(feedbackHp * resAmount,
                                                      0.18f + sat * 0.35f + accent * 0.25f);
        stageIn = asymLimit(stageIn, 0.25f + sat * 0.55f + accent * 0.25f);

        for (int i = 0; i < 3; ++i)
        {
            const float stageG = i == 2 ? brightG : G;
            const float v = (stageIn - z[(size_t) i]) * stageG;
            const float y = v + z[(size_t) i];
            z[(size_t) i] = y + v;

            if (std::abs(z[(size_t) i]) < 1.0e-20f)
                z[(size_t) i] = 0.f;

            stageIn = asymLimit(y, 0.12f + sat * 0.35f + accent * 0.12f);
        }

        const float resonantTap = z[1] - z[2];
        float out = z[2] * (1.0f + res * 0.10f + accent * 0.14f)
                  + resonantTap * resCurve * (1.65f + accent * 0.80f);
        out *= 1.f + params.bassComp * res * 0.35f;
        out = asymLimit(out, sat * 0.3f + accent * 0.16f);
        return FilterModelDetail::clampAudio(out);
    }
};

class SequentialLP final : public IFilter
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
        feedbackMemory = 0.f;
        previousOutput = 0.f;
        cutoffMemory = 1000.f;
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
    struct Character
    {
        float cutoffScale = 1.f;
        float maxCutoff = 20000.f;
        float feedback = 3.75f;
        float feedbackMorph = 0.f;
        float resonanceCurve = 1.f;
        float maxResonance = 1.f;
        float inputBase = 0.9f;
        float inputRange = 3.f;
        float inputSaturation = 0.25f;
        float stageSaturation = 0.12f;
        float leakage = 0.9995f;
        float cutoffSlew = 0.6f;
        float feedbackSlew = 1.f;
        float outputSmoothing = 0.f;
        float highComp = 0.f;
        float outputGain = 1.f;
        float bassComp = 0.35f;
        float compression = 0.f;
        float stageSpread = 0.f;
        int poles = 4;
        bool asymmetric = false;
        bool prophet6Slope = false;
        bool aggressiveCutoff = false;
    };

    double sr = 48000.0;
    std::array<float, 4> z {};
    float feedbackMemory = 0.f;
    float previousOutput = 0.f;
    float cutoffMemory = 1000.f;

    static Character getCharacter(FilterMode mode)
    {
        switch (mode)
        {
            case FilterMode::SequentialP5Rev2:
                // SSM 2040 style: liquid resonance, softer clipping and
                // slightly imperfect component spread.
                return { 0.90f, 18000.f, 3.55f, 0.20f, 1.12f, 0.98f,
                         0.78f, 2.45f, 0.30f, 0.18f, 0.9992f, 0.34f,
                         0.36f, 0.035f, 0.07f, 0.98f, 0.48f, 0.16f,
                         0.018f, 4, false, false, false };

            case FilterMode::SequentialP5Rev3:
                // CEM 3320 Prophet-5 Rev 3: brighter, faster and punchier.
                return { 1.0f, 20000.f, 3.90f, 0.28f, 0.92f, 1.0f,
                         0.92f, 3.25f, 0.18f, 0.10f, 0.9998f, 0.82f,
                         1.0f, 0.01f, 0.11f, 1.02f, 0.35f, 0.035f,
                         0.006f, 4, false, false, false };

            case FilterMode::SequentialP600:
                // Same chip family as Rev 3, but darker surrounding values and
                // slower envelope/filter response.
                return { 0.72f, 16500.f, 3.35f, 0.10f, 1.05f, 0.92f,
                         0.66f, 2.15f, 0.24f, 0.16f, 0.9987f, 0.16f,
                         0.42f, 0.18f, 0.0f, 0.88f, 0.42f, 0.08f,
                         0.012f, 4, false, false, false };

            case FilterMode::SequentialP6:
                // Modern SSI/discrete take: controlled resonance with variable
                // slope on Morph, inspired by the Prophet-6 2/4-pole option.
                return { 1.0f, 22000.f, 3.78f, 0.12f, 0.98f, 0.99f,
                         0.92f, 2.80f, 0.16f, 0.10f, 0.9997f, 0.78f,
                         1.0f, 0.015f, 0.025f, 1.0f, 0.30f, 0.025f,
                         0.004f, 4, false, true, false };

            case FilterMode::SequentialProOne:
                // Hot CEM monosynth voice: fast response, aggressive feedback
                // and a little asymmetric output behavior.
                return { 1.02f, 20000.f, 4.02f, 0.42f, 1.35f, 1.0f,
                         1.08f, 4.90f, 0.32f, 0.20f, 0.9999f, 1.0f,
                         1.0f, 0.0f, 0.08f, 1.04f, 0.32f, 0.02f,
                         0.006f, 4, true, false, true };

            case FilterMode::LP12:
            case FilterMode::LP24:
            case FilterMode::HP12:
            case FilterMode::HP24:
            case FilterMode::Bandpass:
            case FilterMode::Notch:
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
            case FilterMode::RolandJuno106:
            case FilterMode::RolandJuno60:
            case FilterMode::RolandJupiter4:
            case FilterMode::RolandJupiter8:
            case FilterMode::TB303:
            case FilterMode::NumModes:
                return {};
        }

        return {};
    }

    static float softLimit(float x, float amount)
    {
        amount = juce::jlimit(0.f, 1.f, amount);
        if (amount <= 0.0001f)
            return x;

        const float drive = 1.f + amount * 3.5f;
        return std::tanh(x * drive) / drive;
    }

    static float asymLimit(float x, float amount)
    {
        const float bias = amount * 0.025f;
        return softLimit(x + bias, amount) - bias * 0.65f;
    }

    static float logNormaliseCutoff(float cutoff)
    {
        const float hz = juce::jlimit(20.f, 22000.f, cutoff);
        return juce::jlimit(0.f, 1.f, std::log(hz / 20.f) / std::log(1100.f));
    }

    static float onePoleG(float cutoff, double sampleRate)
    {
        const float g = std::tan(juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
        return g / (1.f + g);
    }

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const Character c = getCharacter(params.mode);
        const float morph = juce::jlimit(0.f, 1.f, params.morph);
        const float cutoffNorm = logNormaliseCutoff(params.cutoff);
        float targetCutoff = params.cutoff * c.cutoffScale;

        if (c.aggressiveCutoff)
            targetCutoff = 20.f * std::pow(c.maxCutoff / 20.f, std::pow(cutoffNorm, 1.18f));

        targetCutoff *= std::pow(2.f, (morph - 0.5f) * c.stageSpread * 5.f);
        targetCutoff = juce::jlimit(20.f,
                                    juce::jmin(c.maxCutoff, (float) coreSr * 0.42f),
                                    targetCutoff);
        cutoffMemory += (targetCutoff - cutoffMemory) * c.cutoffSlew;

        const float baseCutoff = juce::jlimit(20.f,
                                              juce::jmin(c.maxCutoff, (float) coreSr * 0.42f),
                                              cutoffMemory);
        const int poles = c.prophet6Slope ? 2 + juce::jlimit(0, 2, juce::roundToInt(morph * 2.f))
                                          : c.poles;
        const float res = juce::jlimit(0.f, c.maxResonance, params.resonance);
        const float feedback = std::pow(res, c.resonanceCurve) * (c.feedback + morph * c.feedbackMorph);
        const float sat = juce::jlimit(0.f, 1.f, params.saturation);

        const float feedbackSource = z[(size_t) poles - 1];
        if (c.feedbackSlew < 0.999f)
            feedbackMemory += (feedbackSource - feedbackMemory) * c.feedbackSlew;
        else
            feedbackMemory = feedbackSource;

        const float driveGain = c.inputBase + params.drive * c.inputRange + morph * 0.35f;
        float stageIn = input * driveGain - softLimit(feedbackMemory * feedback,
                                                      sat * 0.35f + c.stageSaturation);
        stageIn = c.asymmetric ? asymLimit(stageIn, sat * 0.65f + c.inputSaturation)
                               : softLimit(stageIn, sat * 0.55f + c.inputSaturation);

        const float tolerance = (morph - 0.5f) * c.stageSpread;
        for (int i = 0; i < poles; ++i)
        {
            const float stageOffset = 1.f + tolerance * ((float) i - 1.5f);
            const float stageCutoff = juce::jlimit(20.f,
                                                   (float) coreSr * 0.42f,
                                                   baseCutoff * stageOffset);
            const float G = onePoleG(stageCutoff, coreSr);
            const float v = (stageIn - z[(size_t) i]) * G;
            const float y = v + z[(size_t) i];
            z[(size_t) i] = (y + v) * c.leakage;

            if (std::abs(z[(size_t) i]) < 1.0e-20f)
                z[(size_t) i] = 0.f;

            stageIn = c.asymmetric ? asymLimit(y, sat * 0.35f + c.stageSaturation)
                                   : softLimit(y, sat * 0.30f + c.stageSaturation);
        }

        float out = z[(size_t) poles - 1] * c.outputGain;
        if (c.highComp > 0.f)
            out += (out - previousOutput) * c.highComp;

        if (c.outputSmoothing > 0.f)
            out = out * (1.f - c.outputSmoothing) + previousOutput * c.outputSmoothing;

        if (params.mode == FilterMode::SequentialProOne && res > 0.78f)
            out += out * out * out * (res - 0.78f) * 0.45f;

        out *= 1.f + params.bassComp * res * c.bassComp;
        if (c.compression > 0.f)
            out /= 1.f + std::abs(out) * c.compression;

        previousOutput = out;
        out = c.asymmetric ? asymLimit(out, sat * 0.3f + c.stageSaturation)
                           : softLimit(out, sat * 0.25f + c.stageSaturation);
        return FilterModelDetail::clampAudio(out);
    }
};

class RolandLP final : public IFilter
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
        cutoffMemory = 1000.f;
        outputMemory = 0.f;
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
    struct Character
    {
        float cutoffScale = 1.f;
        float maxCutoff = 20000.f;
        float feedback = 3.65f;
        float resonanceCurve = 1.f;
        float inputBase = 0.95f;
        float inputRange = 3.5f;
        float inputSaturation = 0.35f;
        float stageSaturation = 0.18f;
        float leakage = 0.9995f;
        float outputGain = 1.f;
        float outputMemoryMix = 0.f;
        float bassComp = 0.45f;
        float compression = 0.f;
        float slew = 1.f;
        bool asymmetric = false;
    };

    double sr = 48000.0;
    std::array<float, 4> z {};
    float cutoffMemory = 1000.f;
    float outputMemory = 0.f;

    static Character getCharacter(FilterMode mode)
    {
        switch (mode)
        {
            case FilterMode::RolandJuno106:
                // 80017A-inspired: smooth, lightly compressed and a bit less
                // aggressive than the discrete IR3109 voices.
                return { 0.92f, 18000.f, 3.45f, 1.08f, 0.80f, 3.15f,
                         0.30f, 0.16f, 0.9994f, 0.98f, 0.06f, 0.34f,
                         0.18f, 1.f, false };

            case FilterMode::RolandJuno60:
                // IR3109 with warmer surrounding gain staging and gentler
                // movement, so sweeps feel a little rounded.
                return { 0.84f, 17000.f, 3.68f, 1.0f, 0.88f, 3.35f,
                         0.28f, 0.15f, 0.9991f, 0.95f, 0.09f, 0.45f,
                         0.08f, 0.18f, false };

            case FilterMode::RolandJupiter4:
                // Darker, dirtier early-Roland flavor with more drive and a
                // limited top end.
                return { 0.68f, 12000.f, 3.82f, 0.92f, 1.10f, 4.80f,
                         0.48f, 0.28f, 0.9975f, 0.88f, 0.13f, 0.55f,
                         0.05f, 0.10f, true };

            case FilterMode::RolandJupiter8:
                // Cleaner, brighter and faster IR3109 response.
                return { 1.0f, 20000.f, 3.90f, 0.98f, 0.82f, 2.65f,
                         0.20f, 0.10f, 0.9998f, 1.02f, 0.015f, 0.35f,
                         0.02f, 1.f, false };

            case FilterMode::LP12:
            case FilterMode::LP24:
            case FilterMode::HP12:
            case FilterMode::HP24:
            case FilterMode::Bandpass:
            case FilterMode::Notch:
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
            case FilterMode::TB303:
            case FilterMode::SequentialP5Rev2:
            case FilterMode::SequentialP5Rev3:
            case FilterMode::SequentialP600:
            case FilterMode::SequentialP6:
            case FilterMode::SequentialProOne:
            case FilterMode::NumModes:
                return {};
        }

        return {};
    }

    static float softLimit(float x, float amount)
    {
        amount = juce::jlimit(0.f, 1.f, amount);
        if (amount <= 0.0001f)
            return x;

        const float drive = 1.f + amount * 3.f;
        return std::tanh(x * drive) / drive;
    }

    static float asymmetricClip(float x, float amount)
    {
        const float shaped = softLimit(x + amount * 0.045f, amount);
        return shaped - amount * 0.025f;
    }

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const Character c = getCharacter(params.mode);
        const float targetCutoff = juce::jlimit(20.f,
                                                juce::jmin(c.maxCutoff, (float) coreSr * 0.42f),
                                                params.cutoff * c.cutoffScale);
        cutoffMemory += (targetCutoff - cutoffMemory) * c.slew;

        const float cutoff = juce::jlimit(20.f,
                                          juce::jmin(c.maxCutoff, (float) coreSr * 0.42f),
                                          cutoffMemory);
        const float g = std::tan(juce::MathConstants<float>::pi * cutoff / (float) coreSr);
        const float G = g / (1.f + g);
        const float res = juce::jlimit(0.f, 1.f, params.resonance);
        const float resCurve = std::pow(res, c.resonanceCurve * 0.78f);
        const float feedback = resCurve * c.feedback;
        const float sat = juce::jlimit(0.f, 1.f, params.saturation);

        const float fb = softLimit(z[3] * feedback, sat * 0.5f + c.stageSaturation);
        const float driveGain = c.inputBase + params.drive * c.inputRange;
        float stageIn = input * driveGain - fb;
        stageIn = c.asymmetric ? asymmetricClip(stageIn, sat * 0.75f + c.inputSaturation)
                               : softLimit(stageIn, sat * 0.65f + c.inputSaturation);

        for (int i = 0; i < 4; ++i)
        {
            const float v = (stageIn - z[(size_t) i]) * G;
            const float y = v + z[(size_t) i];
            z[(size_t) i] = (y + v) * c.leakage;

            if (std::abs(z[(size_t) i]) < 1.0e-20f)
                z[(size_t) i] = 0.f;

            stageIn = c.asymmetric ? asymmetricClip(y, sat * 0.35f + c.stageSaturation)
                                   : softLimit(y, sat * 0.35f + c.stageSaturation);
        }

        const float resonantTap = (z[2] - z[3]) + (z[1] - z[2]) * 0.35f;
        float out = (z[3] + resonantTap * resCurve * (0.85f + c.stageSaturation)) * c.outputGain;
        out *= 1.f + params.bassComp * res * c.bassComp;

        if (c.compression > 0.f)
            out /= 1.f + std::abs(out) * c.compression;

        if (c.outputMemoryMix > 0.f)
        {
            out = out * (1.f - c.outputMemoryMix) + outputMemory * c.outputMemoryMix;
            outputMemory = out;
        }

        return FilterModelDetail::clampAudio(out);
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
        ic1eq = 0.f;
        ic2eq = 0.f;
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
    float ic1eq = 0.f;
    float ic2eq = 0.f;

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const float cutoff = juce::jlimit(20.f, (float) coreSr * 0.45f, params.cutoff);
        const float g = std::tan(juce::MathConstants<float>::pi * cutoff / (float) coreSr);
        const float damping = juce::jlimit(0.015f, 2.0f, 1.85f - params.resonance * 1.82f);
        const float a1 = 1.f / (1.f + g * (g + damping));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float x = FilterModelDetail::saturate(input * (1.f + params.drive * 5.f),
                                                    params.saturation);

        // Linear trapezoidal / TPT SVF. This produces simultaneous SEM-style
        // 12 dB low, band, high and notch outputs from the same filter state.
        const float v3 = x - ic2eq;
        const float band = a1 * ic1eq + a2 * v3;
        const float low = ic2eq + a2 * ic1eq + a3 * v3;
        const float high = x - damping * band - low;
        ic1eq = 2.f * band - ic1eq;
        ic2eq = 2.f * low - ic2eq;

        if (std::abs(ic1eq) < 1.0e-20f) ic1eq = 0.f;
        if (std::abs(ic2eq) < 1.0e-20f) ic2eq = 0.f;

        const float notch = low + high;
        const float m = juce::jlimit(0.f, 1.f, params.morph);
        const float out = m < 0.5f
            ? FilterModelDetail::lerp(low, notch, m * 2.f)
            : FilterModelDetail::lerp(notch, high, (m - 0.5f) * 2.f);

        const float semLevel = 1.f + params.resonance * 0.18f;
        return FilterModelDetail::clampAudio(
            FilterModelDetail::saturate(out * semLevel, params.saturation * 0.35f));
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
