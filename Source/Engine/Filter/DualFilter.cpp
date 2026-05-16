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

    iirFilter.prepare(spec);
    cleanSVF.prepare(sampleRate);
    ladderLP.prepare(sampleRate);
    semMorph.prepare(sampleRate);
    msDual.prepare(sampleRate);
    combFilter.prepare(sampleRate);
    formantFilter.prepare(sampleRate);

    // Initialize IIR to unity-gain low-pass at Nyquist so first processSample is safe
    // if a Peak/Shelf mode is selected before configureIIR() runs.
    iirFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
        sampleRate,
        static_cast<float>(sampleRate * 0.45),
        juce::MathConstants<float>::sqrt2 * 0.5f);

    cutoffSmooth.reset(sampleRate, 0.01);
    resonanceSmooth.reset(sampleRate, 0.01);
    driveSmooth.reset(sampleRate, 0.01);
    morphSmooth.reset(sampleRate, 0.01);
    mixSmooth.reset(sampleRate, 0.01);

    cutoffSmooth.setCurrentAndTargetValue(juce::jlimit(20.f, (float) sr * 0.49f, params.cutoff));
    resonanceSmooth.setCurrentAndTargetValue(juce::jlimit(0.f, 1.f, params.resonance));
    driveSmooth.setCurrentAndTargetValue(juce::jlimit(0.f, 1.f, params.drive));
    morphSmooth.setCurrentAndTargetValue(juce::jlimit(0.f, 1.f, params.morph));
    mixSmooth.setCurrentAndTargetValue(juce::jlimit(0.f, 1.f, params.mix));
    smoothingReady = true;

    reset();
}

void SingleFilter::reset()
{
    iirFilter.reset();
    cleanSVF.reset();
    ladderLP.reset();
    semMorph.reset();
    msDual.reset();
    combFilter.reset();
    formantFilter.reset();

    if (smoothingReady)
    {
        cutoffSmooth.setCurrentAndTargetValue(juce::jlimit(20.f, (float) sr * 0.49f, params.cutoff));
        resonanceSmooth.setCurrentAndTargetValue(juce::jlimit(0.f, 1.f, params.resonance));
        driveSmooth.setCurrentAndTargetValue(juce::jlimit(0.f, 1.f, params.drive));
        morphSmooth.setCurrentAndTargetValue(juce::jlimit(0.f, 1.f, params.morph));
        mixSmooth.setCurrentAndTargetValue(juce::jlimit(0.f, 1.f, params.mix));
    }

    lastConfiguredCutoff = -1.f;
    lastConfiguredRes = -1.f;
    lastConfiguredDrive = -1.f;
    lastConfiguredMode = FilterMode::NumModes;
}

void SingleFilter::setParams(const Params& p)
{
    params = p;
    params.cutoff       = juce::jlimit(20.f, (float) sr * 0.49f, params.cutoff);
    params.resonance    = juce::jlimit(0.f, 1.f, params.resonance);
    params.drive        = juce::jlimit(0.f, 1.f, params.drive);
    params.morph        = juce::jlimit(0.f, 1.f, params.morph);
    params.saturation   = juce::jlimit(0.f, 1.f, params.saturation);
    params.bassComp     = juce::jlimit(0.f, 1.f, params.bassComp);
    params.outputGainDb = juce::jlimit(-24.f, 24.f, params.outputGainDb);
    params.mix          = juce::jlimit(0.f, 1.f, params.mix);
    params.oversample   = juce::jlimit(0, 2, params.oversample);

    if (smoothingReady)
    {
        cutoffSmooth.setTargetValue(params.cutoff);
        resonanceSmooth.setTargetValue(params.resonance);
        driveSmooth.setTargetValue(params.drive);
        morphSmooth.setTargetValue(params.morph);
        mixSmooth.setTargetValue(params.mix);
    }
}

float SingleFilter::computeCutoff(float baseCutoff, float envValue, float lfoValue) const
{
    // Env: add up to 4 octaves (env amount ×±4 octaves)
    float envOctaves = params.envAmount * 4.f * envValue;
    // LFO: add up to 2 octaves
    float lfoOctaves = params.lfoAmount * 2.f * lfoValue;
    float modulated  = baseCutoff * std::pow(2.f, envOctaves + lfoOctaves);
    return juce::jlimit(20.f, (float)(sr * 0.49), modulated);
}

bool SingleFilter::needsReconfigure(float cutHz, float res, float drive) const
{
    return params.mode != lastConfiguredMode
        || std::abs(cutHz - lastConfiguredCutoff) > 0.5f
        || std::abs(res - lastConfiguredRes) > 0.0001f
        || std::abs(drive - lastConfiguredDrive) > 0.0001f;
}

void SingleFilter::markConfigured(float cutHz, float res, float drive)
{
    lastConfiguredCutoff = cutHz;
    lastConfiguredRes = res;
    lastConfiguredDrive = drive;
    lastConfiguredMode = params.mode;
}

void SingleFilter::configureIIR(float cutHz, float res, float drive)
{
    if (!needsReconfigure(cutHz, res, drive))
        return;

    juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
    float q = 0.707f + res * 10.f;

    switch (params.mode)
    {
        case FilterMode::Peak:
            coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                         sr, cutHz, q, 1.f + drive * 12.f);
            break;
        case FilterMode::LowShelf:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                         sr, cutHz, q, 1.f + drive * 6.f);
            break;
        case FilterMode::HighShelf:
            coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                         sr, cutHz, q, 1.f + drive * 6.f);
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
        case FilterMode::Ladder12:
        case FilterMode::Ladder18:
        case FilterMode::Ladder24:
        case FilterMode::SEMMorph:
        case FilterMode::MSDual:
        case FilterMode::Formant:
        case FilterMode::NumModes:
            break;
    }

    if (coeffs != nullptr)
        *iirFilter.coefficients = *coeffs;

    markConfigured(cutHz, res, drive);
}

float SingleFilter::processSample(float input, float envValue, float lfoValue, float amMod)
{
    const float baseCutoff = smoothingReady ? cutoffSmooth.getNextValue() : params.cutoff;
    const float res        = smoothingReady ? resonanceSmooth.getNextValue() : params.resonance;
    const float drive      = smoothingReady ? driveSmooth.getNextValue() : params.drive;
    const float morph      = smoothingReady ? morphSmooth.getNextValue() : params.morph;
    const float mix        = smoothingReady ? mixSmooth.getNextValue() : params.mix;

    float cutHz = computeCutoff(baseCutoff, envValue, lfoValue);
    FilterProcessParams fp;
    fp.mode = params.mode;
    fp.cutoff = cutHz;
    fp.resonance = res;
    fp.drive = drive;
    fp.morph = morph;
    fp.saturation = params.saturation;
    fp.bassComp = params.bassComp;
    fp.outputGainDb = params.outputGainDb;
    fp.mix = mix;
    fp.oversample = params.oversample;

    float driven = (drive > 0.f) ? std::tanh(input * (1.f + drive * 4.f)) : input;

    float out = driven;

    switch (params.mode)
    {
        case FilterMode::LP12:
        case FilterMode::HP12:
        case FilterMode::Bandpass:
        case FilterMode::LP24:
        case FilterMode::HP24:
        case FilterMode::Notch:
            out = cleanSVF.processSample(driven, fp);
            break;

        case FilterMode::Ladder12:
        case FilterMode::Ladder18:
        case FilterMode::Ladder24:
            out = ladderLP.processSample(input, fp);
            break;

        case FilterMode::SEMMorph:
            out = semMorph.processSample(input, fp);
            break;

        case FilterMode::MSDual:
            out = msDual.processSample(input, fp);
            break;

        case FilterMode::Formant:
            out = formantFilter.processSample(input, fp);
            break;

        case FilterMode::Peak:
        case FilterMode::LowShelf:
        case FilterMode::HighShelf:
            configureIIR(cutHz, res, drive);
            out = iirFilter.processSample(driven);
            break;

        case FilterMode::Comb:
            out = combFilter.processSample(input, fp);
            break;

        case FilterMode::AM_Plus:
            out = driven * (1.f + amMod * params.envAmount);
            break;

        case FilterMode::AM_Minus:
            out = driven * juce::jlimit(-1.f, 1.f, amMod);
            break;

        case FilterMode::NumModes:
            break;
    }

    const float gain = juce::Decibels::decibelsToGain(params.outputGainDb);
    out = input + (out * gain - input) * mix;

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
