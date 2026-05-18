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
            case FilterMode::GliesePrism:
            case FilterMode::AltLadder:
            case FilterMode::PhysicalLadder:
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
            case FilterMode::GliesePrism:
            case FilterMode::AltLadder:
            case FilterMode::PhysicalLadder:
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

class AlternateLadderLP final : public IFilter
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
        cutoffMemory = 1000.f;
        energyMemory = 0.f;
        lastOutput = 0.f;
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
    float feedbackMemory = 0.f;
    float cutoffMemory = 1000.f;
    float energyMemory = 0.f;
    float lastOutput = 0.f;

    static float onePoleG(float cutoff, double sampleRate)
    {
        const float g = std::tan(juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
        return g / (1.f + g);
    }

    static float onePoleExp(float cutoff, double sampleRate)
    {
        return juce::jlimit(0.0001f, 0.995f,
            1.f - std::exp(-2.f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate));
    }

    static float shapedLimit(float x, float amount, float bias)
    {
        amount = juce::jlimit(0.f, 1.f, amount);
        if (amount <= 0.0001f)
            return x;

        const float b = bias * amount;
        const float drive = 1.f + amount * 4.75f;
        return std::tanh((x + b) * drive) / drive - b * 0.55f;
    }

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const bool physical = params.mode == FilterMode::PhysicalLadder;
        const float res = juce::jlimit(0.f, 1.f, params.resonance);
        const float morph = juce::jlimit(0.f, 1.f, params.morph);
        const float sat = juce::jlimit(0.f, 1.f, params.saturation);
        const float cutoffLimit = physical ? 0.40f : 0.43f;
        const float targetCutoff = juce::jlimit(20.f,
                                                (float) coreSr * cutoffLimit,
                                                params.cutoff * (physical ? 0.96f : 1.f));

        if (physical)
            cutoffMemory += (targetCutoff - cutoffMemory) * (0.12f + morph * 0.42f);
        else
            cutoffMemory = targetCutoff;

        const float baseCutoff = juce::jlimit(20.f, (float) coreSr * cutoffLimit, cutoffMemory);
        const float feedbackTap = physical ? z[3] * 0.82f + z[2] * 0.18f
                                           : z[3] * 0.70f + z[2] * 0.30f;
        feedbackMemory += (feedbackTap - feedbackMemory) * (physical ? 0.38f + morph * 0.36f : 1.f);

        const float resCurve = std::pow(res, physical ? 0.72f : 0.58f);
        float feedback = resCurve * (physical ? 4.18f : 4.45f);
        feedback += morph * res * (physical ? 0.60f : 0.42f);

        const float driveGain = (physical ? 0.88f : 0.95f)
                              + params.drive * (physical ? 6.8f : 5.1f)
                              + morph * (physical ? 0.55f : 0.20f);
        const float feedbackShape = physical ? sat * 0.42f + res * 0.10f
                                             : sat * 0.16f + res * 0.035f;
        float stageIn = input * driveGain - shapedLimit(feedbackMemory * feedback,
                                                        feedbackShape,
                                                        physical ? 0.055f : 0.f);
        stageIn = shapedLimit(stageIn, sat * (physical ? 0.75f : 0.55f) + params.drive * 0.12f,
                              physical ? 0.045f : 0.f);

        static constexpr float spread[4] { -0.34f, -0.08f, 0.16f, 0.31f };
        for (int i = 0; i < 4; ++i)
        {
            const float stageOffset = physical
                ? 1.f + spread[(size_t) i] * (0.08f + morph * 0.22f)
                : 1.f + spread[(size_t) i] * morph * 0.08f;
            const float stageCutoff = juce::jlimit(20.f,
                                                   (float) coreSr * cutoffLimit,
                                                   baseCutoff * stageOffset);
            const float G = onePoleG(stageCutoff, coreSr);
            const float v = (stageIn - z[(size_t) i]) * G;
            const float y = v + z[(size_t) i];
            z[(size_t) i] = (y + v) * (physical ? 0.9991f - morph * 0.00035f : 0.99985f);

            if (std::abs(z[(size_t) i]) < 1.0e-20f)
                z[(size_t) i] = 0.f;

            stageIn = shapedLimit(y,
                                  sat * (physical ? 0.34f : 0.24f) + res * 0.03f,
                                  physical ? 0.018f * ((float) i + 1.f) : 0.f);
        }

        const float lp12 = z[1];
        const float lp18 = z[2];
        const float lp24 = z[3];
        float out = morph < 0.5f
            ? FilterModelDetail::lerp(lp12, lp18, morph * 2.f)
            : FilterModelDetail::lerp(lp18, lp24, (morph - 0.5f) * 2.f);

        const float poleSpread = (z[0] - z[1]) * 0.20f
                               + (z[1] - z[2]) * (0.52f + morph * 0.20f)
                               + (z[2] - z[3]) * (0.32f + morph * 0.28f);
        const float biteTap = physical
            ? (z[1] - z[3]) * res * 0.34f
            : poleSpread * resCurve * (0.82f + morph * 0.46f);
        out += biteTap;

        const float bassG = onePoleExp(55.f + params.bassComp * 180.f, coreSr);
        energyMemory += bassG * (z[3] - energyMemory);
        out += energyMemory * params.bassComp * res * (physical ? 0.82f : 0.55f);

        if (physical)
            out += (out - lastOutput) * (0.012f + morph * 0.020f);

        lastOutput = out;
        out = shapedLimit(out * (1.f + res * (physical ? 0.10f : 0.18f)),
                          sat * (physical ? 0.42f : 0.30f) + morph * (physical ? 0.10f : 0.04f),
                          physical ? 0.020f : 0.f);
        return FilterModelDetail::clampAudio(out);
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
        lastOutput = 0.f;
        bassState = 0.f;
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
    float lastOutput = 0.f;
    float bassState = 0.f;

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

    static float diodeLimit(float x, float amount, float asymmetry)
    {
        amount = juce::jlimit(0.f, 1.f, amount);
        asymmetry = juce::jlimit(0.f, 1.f, asymmetry);

        if (amount <= 0.0001f)
            return x;

        const float bias = amount * asymmetry * 0.075f;
        const float drive = 1.f + amount * 5.75f;
        return std::tanh((x + bias) * drive) / drive - bias * 0.45f;
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

    static float onePoleExp(float cutoff, double sampleRate)
    {
        return juce::jlimit(0.0001f, 0.995f,
            1.f - std::exp(-2.f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate));
    }

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const float accent = juce::jlimit(0.f, 1.f, params.morph);
        const float res = juce::jlimit(0.f, 1.f, params.resonance);
        const float resCurve = std::pow(res, 0.54f);
        const float sat = juce::jlimit(0.f, 1.f, params.saturation);
        const float cutoffNorm = logNormaliseCutoff(params.cutoff);
        const float acidRangeHz = 48.f * std::pow(230.f, std::pow(cutoffNorm, 0.92f));
        const float selfFmHz = lastOutput * (180.f + params.drive * 720.f + accent * 880.f)
                             * (0.20f + res * 0.80f);
        const float targetCutoff = juce::jlimit(25.f,
                                                juce::jmin(15500.f, (float) coreSr * 0.36f),
                                                acidRangeHz * std::pow(2.f, accent * 0.95f)
                                                    + selfFmHz);
        cutoffMemory += (targetCutoff - cutoffMemory) * (0.10f + accent * 0.42f + params.drive * 0.08f);

        const float cutoff = juce::jlimit(25.f,
                                          juce::jmin(15500.f, (float) coreSr * 0.36f),
                                          cutoffMemory);

        const float feedbackSource = z[2] * 0.72f + lastOutput * 0.28f;
        const float hpHz = 110.f + accent * 760.f + params.drive * 140.f;
        const float hpG = onePoleExp(hpHz, coreSr);
        feedbackLow += hpG * (feedbackSource - feedbackLow);
        const float feedbackHp = feedbackSource - feedbackLow;

        float resAmount = resCurve * (3.40f + accent * 1.45f + params.drive * 0.30f);
        if (res > 0.92f)
            resAmount += (res - 0.92f) * (1.40f + accent * 1.20f);

        const float driveGain = 0.82f + params.drive * 5.80f + accent * 1.55f;
        const float biteFeedback = (z[1] - z[2]) * resCurve * (0.65f + accent * 0.55f);
        const float feedback = diodeLimit(feedbackHp * resAmount + biteFeedback,
                                          0.20f + sat * 0.38f + accent * 0.22f,
                                          0.85f);

        float stageIn = input * driveGain - feedback;
        stageIn = diodeLimit(stageIn, 0.30f + sat * 0.55f + accent * 0.26f, 0.72f);

        for (int i = 0; i < 3; ++i)
        {
            const float stageTilt = i == 0 ? 0.88f
                                  : i == 1 ? 1.0f + accent * 0.04f
                                           : 1.16f + accent * 0.34f;
            const float stageCutoff = juce::jlimit(25.f,
                                                   (float) coreSr * 0.42f,
                                                   cutoff * stageTilt * (1.f + sat * ((float) i - 1.f) * 0.035f));
            const float stageG = onePoleG(stageCutoff, coreSr);
            const float v = (stageIn - z[(size_t) i]) * stageG;
            const float y = v + z[(size_t) i];
            z[(size_t) i] = (y + v) * 0.99965f;

            if (std::abs(z[(size_t) i]) < 1.0e-20f)
                z[(size_t) i] = 0.f;

            stageIn = diodeLimit(y, 0.13f + sat * 0.33f + accent * 0.13f, 0.45f);
        }

        const float resonantTap = (z[1] - z[2]) + (z[0] - z[2]) * 0.10f;
        float out = z[2] * (1.0f + res * 0.10f + accent * 0.14f)
                  + resonantTap * resCurve * (1.80f + accent * 0.92f);

        const float bassG = onePoleExp(70.f + params.bassComp * 130.f, coreSr);
        bassState += bassG * (z[2] - bassState);
        out += bassState * params.bassComp * (0.38f + res * 0.42f);

        out = diodeLimit(out, sat * 0.30f + accent * 0.18f, 0.52f);
        lastOutput = out;
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
            case FilterMode::GliesePrism:
            case FilterMode::AltLadder:
            case FilterMode::PhysicalLadder:
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
        feedbackMemory = 0.f;
        inputCouplingState = 0.f;
        previousOutput = 0.f;
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
        float stageSpread = 0.f;
        float feedbackSlew = 1.f;
        float inputCouplingHz = 18.f;
        float inputCouplingDepth = 0.f;
        float resonantTap = 0.9f;
        float highLift = 0.f;
        float morphCutoffOctaves = 0.f;
        float otaSkew = 0.f;
        bool asymmetric = false;
        bool physicalJupiter = false;
    };

    double sr = 48000.0;
    std::array<float, 4> z {};
    float cutoffMemory = 1000.f;
    float outputMemory = 0.f;
    float feedbackMemory = 0.f;
    float inputCouplingState = 0.f;
    float previousOutput = 0.f;

    static Character getCharacter(FilterMode mode)
    {
        switch (mode)
        {
            case FilterMode::RolandJuno106:
                // 80017A voice chip: smoother OTA drive, a little card-to-card
                // smoothing, and restrained feedback gain.
                return { 0.94f, 18500.f, 3.52f, 1.06f, 0.78f, 3.05f,
                         0.26f, 0.135f, 0.9995f, 0.99f, 0.035f, 0.38f,
                         0.16f, 0.92f, 0.030f, 0.54f, 18.f, 0.07f,
                         0.90f, 0.006f, 0.10f, 0.08f, false, false };

            case FilterMode::RolandJuno60:
                // Discrete IR3109 support circuit: warmer coupling, slower
                // cutoff movement and slightly imperfect OTA stage matching.
                return { 0.86f, 17200.f, 3.74f, 0.98f, 0.88f, 3.45f,
                         0.30f, 0.17f, 0.9991f, 0.96f, 0.075f, 0.50f,
                         0.09f, 0.24f, 0.070f, 0.30f, 24.f, 0.11f,
                         1.02f, 0.0f, 0.18f, 0.17f, false, false };

            case FilterMode::RolandJupiter4:
                // Earlier, darker voice card. The "physical" part here is the
                // stronger input coupling, lossy stages and skewed feedback tap.
                return { 0.70f, 12500.f, 4.04f, 0.88f, 1.05f, 5.05f,
                         0.52f, 0.30f, 0.9972f, 0.88f, 0.14f, 0.64f,
                         0.055f, 0.12f, 0.115f, 0.20f, 42.f, 0.19f,
                         1.24f, -0.020f, 0.30f, 0.34f, true, true };

            case FilterMode::RolandJupiter8:
                // Brighter IR3109 card with faster settling and less lossy
                // coupling than the Jupiter-4 model.
                return { 1.04f, 21000.f, 3.92f, 0.95f, 0.82f, 2.85f,
                         0.20f, 0.105f, 0.99975f, 1.04f, 0.018f, 0.40f,
                         0.025f, 0.92f, 0.040f, 0.74f, 14.f, 0.045f,
                         0.88f, 0.020f, 0.08f, 0.06f, false, true };

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
            case FilterMode::GliesePrism:
            case FilterMode::AltLadder:
            case FilterMode::PhysicalLadder:
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

    static float onePoleG(float cutoff, double sampleRate)
    {
        const float g = std::tan(juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
        return g / (1.f + g);
    }

    static float onePoleExp(float cutoff, double sampleRate)
    {
        return juce::jlimit(0.0001f, 0.995f,
            1.f - std::exp(-2.f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate));
    }

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const Character c = getCharacter(params.mode);
        const float morph = juce::jlimit(0.f, 1.f, params.morph);
        const float morphBipolar = morph * 2.f - 1.f;
        const float targetCutoff = juce::jlimit(20.f,
                                                juce::jmin(c.maxCutoff, (float) coreSr * 0.42f),
                                                params.cutoff * c.cutoffScale
                                                    * std::pow(2.f, morphBipolar * c.morphCutoffOctaves));
        cutoffMemory += (targetCutoff - cutoffMemory) * c.slew;

        const float cutoff = juce::jlimit(20.f,
                                          juce::jmin(c.maxCutoff, (float) coreSr * 0.42f),
                                          cutoffMemory);
        const float res = juce::jlimit(0.f, 1.f, params.resonance);
        const float resCurve = std::pow(res, c.resonanceCurve * 0.78f);
        const float physicalMorph = c.physicalJupiter ? morphBipolar * 0.22f : 0.f;
        const float feedback = resCurve * (c.feedback + physicalMorph);
        const float sat = juce::jlimit(0.f, 1.f, params.saturation);

        const float couplingG = onePoleExp(c.inputCouplingHz, coreSr);
        inputCouplingState += couplingG * (input - inputCouplingState);
        const float coupledInput = input - inputCouplingState * c.inputCouplingDepth;

        const float feedbackSource = z[3] + (z[2] - z[3]) * c.resonantTap * 0.45f;
        feedbackMemory += (feedbackSource - feedbackMemory) * c.feedbackSlew;
        const float fb = softLimit(feedbackMemory * feedback, sat * 0.5f + c.stageSaturation);
        const float driveGain = c.inputBase + params.drive * c.inputRange;
        float stageIn = coupledInput * driveGain - fb;
        stageIn = c.asymmetric ? asymmetricClip(stageIn, sat * 0.75f + c.inputSaturation)
                               : softLimit(stageIn, sat * 0.65f + c.inputSaturation);

        static constexpr float trim[4] { -0.55f, 0.22f, -0.12f, 0.45f };
        const float tolerance = morphBipolar * c.stageSpread;
        for (int i = 0; i < 4; ++i)
        {
            float stageOffset = 1.f + c.stageSpread * 0.06f * trim[(size_t) i]
                                  + tolerance * (((float) i - 1.5f) * 0.18f);
            if (c.physicalJupiter)
                stageOffset *= 1.f + res * 0.018f * ((float) i - 1.5f);

            const float stageCutoff = juce::jlimit(20.f,
                                                   (float) coreSr * 0.42f,
                                                   cutoff * stageOffset);
            const float G = onePoleG(stageCutoff, coreSr);
            const float otaInput = stageIn + c.otaSkew * z[(size_t) i] * z[(size_t) i];
            const float v = (stageIn - z[(size_t) i]) * G;
            const float y = v + z[(size_t) i] + (otaInput - stageIn) * G * 0.20f;
            z[(size_t) i] = (y + v) * c.leakage;

            if (std::abs(z[(size_t) i]) < 1.0e-20f)
                z[(size_t) i] = 0.f;

            stageIn = c.asymmetric ? asymmetricClip(y, sat * 0.35f + c.stageSaturation)
                                   : softLimit(y, sat * 0.35f + c.stageSaturation);
        }

        const float resonantTap = (z[2] - z[3]) * c.resonantTap + (z[1] - z[2]) * 0.30f;
        float out = (z[3] + resonantTap * resCurve * (0.80f + c.stageSaturation)) * c.outputGain;

        if (c.highLift != 0.f)
            out += (out - previousOutput) * c.highLift;

        out *= 1.f + params.bassComp * res * c.bassComp;

        if (c.compression > 0.f)
            out /= 1.f + std::abs(out) * c.compression;

        if (c.outputMemoryMix > 0.f)
        {
            out = out * (1.f - c.outputMemoryMix) + outputMemory * c.outputMemoryMix;
            outputMemory = out;
        }

        previousOutput = out;
        out = c.asymmetric ? asymmetricClip(out, sat * 0.24f + c.stageSaturation * 0.55f)
                           : softLimit(out, sat * 0.20f + c.stageSaturation * 0.50f);
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

class GliesePrismFilter final : public IFilter
{
public:
    void prepare(double sampleRate) override
    {
        sr = sampleRate;
        reset();
    }

    void reset() override
    {
        coreA = {};
        coreB = {};
        bodyState = 0.f;
        energyState = 0.f;
        lastOutput = 0.f;
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
    struct SvfCore
    {
        float ic1eq = 0.f;
        float ic2eq = 0.f;
    };

    struct SvfOut
    {
        float low = 0.f;
        float band = 0.f;
        float high = 0.f;
    };

    double sr = 48000.0;
    SvfCore coreA;
    SvfCore coreB;
    float bodyState = 0.f;
    float energyState = 0.f;
    float lastOutput = 0.f;

    static float onePoleExp(float cutoff, double sampleRate)
    {
        return juce::jlimit(0.0001f, 0.995f,
            1.f - std::exp(-2.f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate));
    }

    static SvfOut processSvf(SvfCore& core, float input, float cutoff, float damping, double sampleRate)
    {
        cutoff = juce::jlimit(20.f, (float) sampleRate * 0.43f, cutoff);
        damping = juce::jlimit(0.035f, 2.0f, damping);

        const float g = std::tan(juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
        const float a1 = 1.f / (1.f + g * (g + damping));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float v3 = input - core.ic2eq;
        const float band = a1 * core.ic1eq + a2 * v3;
        const float low = core.ic2eq + a2 * core.ic1eq + a3 * v3;
        const float high = input - damping * band - low;

        core.ic1eq = 2.f * band - core.ic1eq;
        core.ic2eq = 2.f * low - core.ic2eq;

        if (std::abs(core.ic1eq) < 1.0e-20f) core.ic1eq = 0.f;
        if (std::abs(core.ic2eq) < 1.0e-20f) core.ic2eq = 0.f;

        return { low, band, high };
    }

    float processCore(float input, const FilterProcessParams& params, double coreSr)
    {
        const float morph = juce::jlimit(0.f, 1.f, params.morph);
        const float res = juce::jlimit(0.f, 1.f, params.resonance);
        const float sat = juce::jlimit(0.f, 1.f, params.saturation);
        const float baseCutoff = juce::jlimit(20.f, (float) coreSr * 0.40f, params.cutoff);

        const float prismSpread = std::pow(2.f, 0.35f + morph * 1.75f);
        const float cutA = baseCutoff * std::pow(2.f, -0.18f - morph * 0.34f);
        const float cutB = juce::jmin(baseCutoff * prismSpread, (float) coreSr * 0.42f);
        const float dampingA = 1.52f - res * 1.43f;
        const float dampingB = 1.28f - res * 1.16f + (0.5f - morph) * 0.22f;

        float x = input + lastOutput * res * (0.06f + morph * 0.10f);
        x = FilterModelDetail::saturate(x * (0.92f + params.drive * 5.6f),
                                        sat * 0.70f + params.drive * 0.18f);

        const auto a = processSvf(coreA, x, cutA, dampingA, coreSr);
        const float crossExcite = a.band * res * (0.24f + morph * 0.38f);
        const auto b = processSvf(coreB,
                                  FilterModelDetail::saturate(x + crossExcite, sat * 0.35f),
                                  cutB,
                                  dampingB,
                                  coreSr);

        const float lowMass = FilterModelDetail::lerp(a.low, (a.low + b.low) * 0.5f, morph * 0.65f);
        const float prismBand = (a.band - b.band) * (0.28f + morph * 0.78f);
        const float glassTap = (b.low - a.low + b.high * 0.18f) * morph;
        float out = lowMass + prismBand * res + glassTap * res * 0.55f;

        const float bodyG = onePoleExp(42.f + params.bassComp * 160.f, coreSr);
        bodyState += bodyG * (lowMass - bodyState);
        out += bodyState * params.bassComp * (0.45f + res * 0.30f);

        const float energyG = onePoleExp(850.f + baseCutoff * 0.18f, coreSr);
        energyState += energyG * (std::abs(out) - energyState);
        out /= 1.f + energyState * (sat * 0.40f + params.drive * 0.18f);

        out = FilterModelDetail::saturate(out * (1.f + res * 0.16f), sat * 0.42f + morph * 0.10f);
        lastOutput = out;
        return FilterModelDetail::clampAudio(out);
    }
};
