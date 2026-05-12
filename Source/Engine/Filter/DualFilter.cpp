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

void SingleFilter::configureSVF(float cutHz, float res)
{
    using SVFType = juce::dsp::StateVariableTPTFilterType;
    float resonance = juce::jlimit(0.5f, 25.f, 0.5f + res * 24.5f);
    svf1.setCutoffFrequency(cutHz);
    svf1.setResonance(resonance);
    svf2.setCutoffFrequency(cutHz);
    svf2.setResonance(resonance);

    switch (params.mode)
    {
        case FilterMode::LP12: case FilterMode::LP24:
            svf1.setType(SVFType::lowPass);
            svf2.setType(SVFType::lowPass);
            break;
        case FilterMode::HP12: case FilterMode::HP24:
            svf1.setType(SVFType::highPass);
            svf2.setType(SVFType::highPass);
            break;
        case FilterMode::Bandpass:
            svf1.setType(SVFType::bandPass);
            break;
        case FilterMode::Notch:
            svf1.setType(SVFType::lowPass);
            svf2.setType(SVFType::highPass);
            break;
        default:
            svf1.setType(SVFType::lowPass);
            break;
    }
}

void SingleFilter::configureIIR(float cutHz, float res)
{
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
        default:
            break;
    }

    if (coeffs != nullptr)
        *iirFilter.coefficients = *coeffs;
}

float SingleFilter::processSample(float input, float envValue, float lfoValue, float amMod)
{
    float cutHz = computeCutoff(envValue, lfoValue);
    float res   = params.resonance;

    // Pre-drive saturation
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
            configureSVF(cutHz, res);
            out = svf2.processSample(0, svf1.processSample(0, driven));
            break;

        case FilterMode::Notch:
        {
            // Notch = LP + HP
            configureSVF(cutHz, res);
            using T = juce::dsp::StateVariableTPTFilterType;
            svf1.setType(T::lowPass);
            svf2.setType(T::highPass);
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
            // Feedback comb
            int delaySamples = juce::jlimit(1, kMaxCombDelaySamples - 1,
                                             (int)(sr / juce::jlimit(20.f, (float)(sr * 0.49), cutHz)));
            int readPos = (combWritePos - delaySamples + kMaxCombDelaySamples) % kMaxCombDelaySamples;
            float delayed = combBuffer[readPos];
            float output  = driven + combFeedback * (0.5f + res * 0.5f) * delayed;
            combBuffer[combWritePos] = output;
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

        default:
            break;
    }

    return out;
}

//==============================================================================
// DualFilter
//==============================================================================

void DualFilter::prepare(double sampleRate)
{
    sr = sampleRate;
    filterA.prepare(sampleRate);
    filterB.prepare(sampleRate);
    envA.prepare(sampleRate);
    envB.prepare(sampleRate);
}

void DualFilter::setFilterA(const SingleFilter::Params& p) { filterA.setParams(p); }
void DualFilter::setFilterB(const SingleFilter::Params& p) { filterB.setParams(p); }
void DualFilter::setRouting(FilterRouting r)               { routing = r; }

void DualFilter::noteOn()  { envA.noteOn();  envB.noteOn(); }
void DualFilter::noteOff() { envA.noteOff(); envB.noteOff(); }
void DualFilter::reset()
{
    filterA.reset(); filterB.reset();
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
            float midL = filterA.processSample(inL, evA, lfoA, amMod);
            float midR = filterA.processSample(inR, evA, lfoA, amMod);
            outL = filterB.processSample(midL, evB, lfoB, amMod);
            outR = filterB.processSample(midR, evB, lfoB, amMod);
            break;
        }
        case FilterRouting::Parallel:
        {
            float aL = filterA.processSample(inL, evA, lfoA, amMod);
            float aR = filterA.processSample(inR, evA, lfoA, amMod);
            float bL = filterB.processSample(inL, evB, lfoB, amMod);
            float bR = filterB.processSample(inR, evB, lfoB, amMod);
            outL = (aL + bL) * 0.5f;
            outR = (aR + bR) * 0.5f;
            break;
        }
        case FilterRouting::AOnly:
            outL = filterA.processSample(inL, evA, lfoA, amMod);
            outR = filterA.processSample(inR, evA, lfoA, amMod);
            break;
        case FilterRouting::BOnly:
            outL = filterB.processSample(inL, evB, lfoB, amMod);
            outR = filterB.processSample(inR, evB, lfoB, amMod);
            break;
        default:
            outL = inL; outR = inR;
            break;
    }
    return { outL, outR };
}
