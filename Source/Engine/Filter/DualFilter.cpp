#include "DualFilter.h"

//==============================================================================
// SingleFilter
//==============================================================================

void SingleFilter::prepare(double sampleRate)
{
    sr = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = 512;
    spec.numChannels      = 1;

    svf1.prepare(spec);
    svf2.prepare(spec);
    iirFilter.prepare(spec);

    // Initialize IIR to unity-gain low-pass at Nyquist so first processSample is safe
    // if a Peak/Shelf mode is selected before configureIIR() runs.
    iirFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
        sampleRate,
        static_cast<float>(sampleRate * 0.45),
        juce::MathConstants<float>::sqrt2 * 0.5f);

    shaper.functionToUse = [](float x) { return std::tanh(x); };
    shaper.prepare(spec);

    combBuffer.fill(0.f);
    reset();
}

void SingleFilter::reset()
{
    svf1.reset();
    svf2.reset();
    iirFilter.reset();
    combBuffer.fill(0.f);
    combWritePos = 0;
    lastConfiguredCutoff = -1.f;
    lastConfiguredRes = -1.f;
    lastConfiguredDrive = -1.f;
    lastConfiguredMode = FilterMode::NumModes;
}

void SingleFilter::setParams(const Params& p)
{
    params = p;
}

float SingleFilter::computeCutoff(float envValue, float lfoValue) const
{
    float base = params.cutoff;
    // Env: add up to 4 octaves (env amount ×±4 octaves)
    float envOctaves = params.envAmount * 4.f * envValue;
    // LFO: add up to 2 octaves
    float lfoOctaves = params.lfoAmount * 2.f * lfoValue;
    float modulated  = base * std::pow(2.f, envOctaves + lfoOctaves);
    return juce::jlimit(20.f, (float)(sr * 0.49), modulated);
}

bool SingleFilter::needsReconfigure(float cutHz, float res, bool includeDrive) const
{
    return params.mode != lastConfiguredMode
        || std::abs(cutHz - lastConfiguredCutoff) > 0.5f
        || std::abs(res - lastConfiguredRes) > 0.0001f
        || (includeDrive && std::abs(params.drive - lastConfiguredDrive) > 0.0001f);
}

void SingleFilter::markConfigured(float cutHz, float res)
{
    lastConfiguredCutoff = cutHz;
    lastConfiguredRes = res;
    lastConfiguredDrive = params.drive;
    lastConfiguredMode = params.mode;
}

void SingleFilter::configureSVF(float cutHz, float res)
{
    if (!needsReconfigure(cutHz, res, false))
        return;

    using SVFType = juce::dsp::StateVariableTPTFilterType;
    // Q range 0.5..20: full self-oscillation and screaming resonance available.
    // Clipping protection comes from inter-stage tanh (LP24/HP24) and the
    // final output limiter — not from attenuating the input here.
    float Q = juce::jlimit(0.5f, 20.f, 0.5f + res * 19.5f);
    svf1.setCutoffFrequency(cutHz);
    svf1.setResonance(Q);
    svf2.setCutoffFrequency(cutHz);
    svf2.setResonance(Q);

    switch (params.mode)
    {
        case FilterMode::LP12: case FilterMode::LP24:
            svf1.setType(SVFType::lowpass);
            svf2.setType(SVFType::lowpass);
            break;
        case FilterMode::HP12: case FilterMode::HP24:
            svf1.setType(SVFType::highpass);
            svf2.setType(SVFType::highpass);
            break;
        case FilterMode::Bandpass:
            svf1.setType(SVFType::bandpass);
            break;
        case FilterMode::Notch:
            svf1.setType(SVFType::lowpass);
            svf2.setType(SVFType::highpass);
            break;
        case FilterMode::Peak:
        case FilterMode::LowShelf:
        case FilterMode::HighShelf:
        case FilterMode::Comb:
        case FilterMode::AM_Plus:
        case FilterMode::AM_Minus:
        case FilterMode::NumModes:
            svf1.setType(SVFType::lowpass);
            break;
    }

    markConfigured(cutHz, res);
}

void SingleFilter::configureIIR(float cutHz, float res)
{
    if (!needsReconfigure(cutHz, res, true))
        return;

    juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
    float q = 0.707f + res * 10.f;

    switch (params.mode)
    {
        case FilterMode::Peak:
            coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                         sr, cutHz, q, 1.f + params.drive * 12.f);
            break;
        case FilterMode::LowShelf:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                         sr, cutHz, q, 1.f + params.drive * 6.f);
            break;
        case FilterMode::HighShelf:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                         sr, cutHz, q, 1.f + params.drive * 6.f);
            break;
        case FilterMode::LP12:
        case FilterMode::LP24:
        case FilterMode::HP12:
        case FilterMode::HP24:
        case FilterMode::Bandpass:
        case FilterMode::Notch:
        case FilterMode::Comb:
        case FilterMode::AM_Plus:
        case FilterMode::AM_Minus:
        case FilterMode::NumModes:
            break;
    }

    if (coeffs != nullptr)
        *iirFilter.coefficients = *coeffs;

    markConfigured(cutHz, res);
}

float SingleFilter::processSample(float input, float envValue, float lfoValue, float amMod)
{
    float cutHz = computeCutoff(envValue, lfoValue);
    float res   = params.resonance;

    float driven = (params.drive > 0.f) ? std::tanh(input * (1.f + params.drive * 4.f)) : input;

    float out = driven;

    switch (params.mode)
    {
        case FilterMode::LP12:
        case FilterMode::HP12:
        case FilterMode::Bandpass:
            configureSVF(cutHz, res);
            out = svf1.processSample(0, driven);
            break;

        case FilterMode::LP24:
        case FilterMode::HP24:
        {
            configureSVF(cutHz, res);
            float mid = svf1.processSample(0, driven);
            // Soft-clip between cascade stages: prevents the second stage amplifying
            // an already-hot signal from the first stage (series compound gain).
            mid = std::tanh(mid);
            out = svf2.processSample(0, mid);
            break;
        }

        case FilterMode::Notch:
        {
            // Notch = LP + HP
            configureSVF(cutHz, res);
            using T = juce::dsp::StateVariableTPTFilterType;
            svf1.setType(T::lowpass);
            svf2.setType(T::highpass);
            float lp = svf1.processSample(0, driven);
            float hp = svf2.processSample(0, driven);
            out = (lp + hp) * 0.5f;
            break;
        }

        case FilterMode::Peak:
        case FilterMode::LowShelf:
        case FilterMode::HighShelf:
            configureIIR(cutHz, res);
            out = iirFilter.processSample(driven);
            break;

        case FilterMode::Comb:
        {
            // Feedback comb — cap feedback strictly below unity to prevent runaway
            int delaySamples = juce::jlimit(1, kMaxCombDelaySamples - 1,
                                             (int)(sr / juce::jlimit(20.f, (float)(sr * 0.49), cutHz)));
            int readPos = (combWritePos - delaySamples + kMaxCombDelaySamples) % kMaxCombDelaySamples;
            float delayed = combBuffer[(size_t)readPos];
            float fb = juce::jlimit(0.f, 0.97f, combFeedback * (0.5f + res * 0.5f));
            float output = driven + fb * delayed;
            combBuffer[(size_t)combWritePos] = output;
            combWritePos = (combWritePos + 1) % kMaxCombDelaySamples;
            out = output;
            break;
        }

        case FilterMode::AM_Plus:
            out = driven * (1.f + amMod * params.envAmount);
            break;

        case FilterMode::AM_Minus:
            out = driven * juce::jlimit(-1.f, 1.f, amMod);
            break;

        case FilterMode::NumModes:
            break;
    }

    // Hard-clip as final safety net: compensation gain should keep output in
    // range most of the time, but this prevents any stray blow-up from reaching
    // downstream stages.
    return juce::jlimit(-1.5f, 1.5f, out);
}

//==============================================================================
// DualFilter
//==============================================================================

void DualFilter::prepare(double sampleRate)
{
    sr = sampleRate;
    for (auto& filter : filterA) filter.prepare(sampleRate);
    for (auto& filter : filterB) filter.prepare(sampleRate);
    envA.prepare(sampleRate);
    envB.prepare(sampleRate);
}

void DualFilter::setFilterA(const SingleFilter::Params& p)
{
    for (auto& filter : filterA) filter.setParams(p);
}

void DualFilter::setFilterB(const SingleFilter::Params& p)
{
    for (auto& filter : filterB) filter.setParams(p);
}

void DualFilter::setRouting(FilterRouting r)               { routing = r; }

void DualFilter::noteOn()  { envA.noteOn();  envB.noteOn(); }
void DualFilter::noteOff() { envA.noteOff(); envB.noteOff(); }
void DualFilter::reset()
{
    for (auto& filter : filterA) filter.reset();
    for (auto& filter : filterB) filter.reset();
    envA.reset();    envB.reset();
}

std::pair<float, float> DualFilter::processSample(float inL, float inR,
                                                   float lfoA, float lfoB,
                                                   float amMod)
{
    float evA = envA.processSample();
    float evB = envB.processSample();

    float outL, outR;
    switch (routing)
    {
        case FilterRouting::Series:
        {
            float midL = filterA[0].processSample(inL, evA, lfoA, amMod);
            float midR = filterA[1].processSample(inR, evA, lfoA, amMod);
            outL = filterB[0].processSample(midL, evB, lfoB, amMod);
            outR = filterB[1].processSample(midR, evB, lfoB, amMod);
            break;
        }
        case FilterRouting::Parallel:
        {
            float aL = filterA[0].processSample(inL, evA, lfoA, amMod);
            float aR = filterA[1].processSample(inR, evA, lfoA, amMod);
            float bL = filterB[0].processSample(inL, evB, lfoB, amMod);
            float bR = filterB[1].processSample(inR, evB, lfoB, amMod);
            outL = (aL + bL) * 0.5f;
            outR = (aR + bR) * 0.5f;
            break;
        }
        case FilterRouting::AOnly:
            outL = filterA[0].processSample(inL, evA, lfoA, amMod);
            outR = filterA[1].processSample(inR, evA, lfoA, amMod);
            break;
        case FilterRouting::BOnly:
            outL = filterB[0].processSample(inL, evB, lfoB, amMod);
            outR = filterB[1].processSample(inR, evB, lfoB, amMod);
            break;
        case FilterRouting::NumRoutings:
            outL = inL; outR = inR;
            break;
    }
    return { outL, outR };
}
