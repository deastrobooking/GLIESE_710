#include "GranularOsc.h"
#include "../DSPUtils.h"

namespace
{
float wrap01(float x)
{
    x -= std::floor(x);
    return x;
}

float wrappedDelta(float to, float from)
{
    float delta = to - from;
    if (delta > 0.5f)
        delta -= 1.f;
    else if (delta < -0.5f)
        delta += 1.f;

    return delta;
}

float clampFinite(float x)
{
    return std::isfinite(x) ? juce::jlimit(-4.f, 4.f, x) : 0.f;
}
}

void GranularOsc::prepare(double sampleRate)
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    generateDefaultSource();
    reset();
}

void GranularOsc::reset()
{
    for (auto& grain : grains)
        grain = {};
    for (auto& mode : bodyModes)
    {
        mode.z1L = mode.z2L = 0.f;
        mode.z1R = mode.z2R = 0.f;
    }

    triggerPhase = 0.f;
    scanPosition = juce::jlimit(0.f, 1.f, params.position);
    burstRemaining = 0;
    nextReverse = false;
    rngState = 0x6d2b79f5u ^ (uint32_t) note * 747796405u;
    bodyDirty = true;
}

void GranularOsc::noteOn(int midiNote, float velocity)
{
    note = midiNote;
    noteVelocity = juce::jlimit(0.f, 1.f, velocity);
    rngState = 0x6d2b79f5u ^ (uint32_t) (midiNote + 1) * 747796405u;
    scanPosition = juce::jlimit(0.f, 1.f, params.position);
    triggerPhase = 0.f;
    burstRemaining = juce::jlimit(1, 48, juce::roundToInt(4.f + params.density * 0.06f));

    if (params.triggerMode == TriggerMode::Burst)
        triggerGrain();
}

void GranularOsc::noteOff()
{
    burstRemaining = 0;
}

void GranularOsc::setFrequency(float hz)
{
    frequency = juce::jlimit(1.f, 20000.f, hz);
}

void GranularOsc::setParams(const Params& newParams)
{
    const auto previousTriggerMode = params.triggerMode;
    const float previousPosition = params.position;

    params = newParams;
    if ((int) params.triggerMode < 0 || (int) params.triggerMode >= (int) TriggerMode::NumModes)
        params.triggerMode = TriggerMode::FreeCloud;
    if ((int) params.window < 0 || (int) params.window >= (int) Window::NumWindows)
        params.window = Window::Hann;
    if ((int) params.bodyType < 0 || (int) params.bodyType >= (int) BodyType::NumBodyTypes)
        params.bodyType = BodyType::Off;
    params.position = juce::jlimit(0.f, 1.f, params.position);
    params.sizeMs = juce::jlimit(5.f, 1000.f, params.sizeMs);
    params.density = juce::jlimit(0.1f, 1200.f, params.density);
    params.pitchSemis = juce::jlimit(-48.f, 48.f, params.pitchSemis);
    params.spray = juce::jlimit(0.f, 1.f, params.spray);
    params.jitter = juce::jlimit(0.f, 1.f, params.jitter);
    params.panSpread = juce::jlimit(0.f, 1.f, params.panSpread);
    params.reverseProb = juce::jlimit(0.f, 1.f, params.reverseProb);
    params.scanSpeed = juce::jlimit(-1.f, 1.f, params.scanSpeed);
    params.freeze = juce::jlimit(0.f, 1.f, params.freeze);
    params.bodyRootSemis = juce::jlimit(-48.f, 48.f, params.bodyRootSemis);
    params.bodySize = juce::jlimit(0.f, 1.f, params.bodySize);
    params.bodyDecay = juce::jlimit(0.f, 1.f, params.bodyDecay);
    params.bodyDamping = juce::jlimit(0.f, 1.f, params.bodyDamping);
    params.bodyBrightness = juce::jlimit(0.f, 1.f, params.bodyBrightness);
    params.bodyInharm = juce::jlimit(0.f, 1.f, params.bodyInharm);
    params.bodyExcite = juce::jlimit(0.f, 1.f, params.bodyExcite);
    params.bodyMix = juce::jlimit(0.f, 1.f, params.bodyMix);
    params.bodySpread = juce::jlimit(0.f, 1.f, params.bodySpread);
    params.bodyModes = juce::jlimit(4, kMaxBodyModes, params.bodyModes);
    bodyDirty = true;

    const float positionDelta = wrappedDelta(params.position, previousPosition);
    if (std::abs(positionDelta) > 0.000001f)
        scanPosition = wrap01(scanPosition + positionDelta);

    scanPosition = wrap01(scanPosition);

    if (params.triggerMode == TriggerMode::Burst && previousTriggerMode != TriggerMode::Burst)
    {
        burstRemaining = juce::jlimit(1, 48, juce::roundToInt(4.f + params.density * 0.06f));
        triggerPhase = 1.f;
    }
}

void GranularOsc::setSample(const float* samples, int numSamples)
{
    if (samples == nullptr || numSamples <= 0)
    {
        generateDefaultSource();
        return;
    }

    const int regionSize = juce::jmax(2, numSamples / kTableFrames);
    std::array<float, kFrameSamples> frame {};

    for (int f = 0; f < kTableFrames; ++f)
    {
        const int frameOffset = f * kFrameSamples;
        const float regionStart = (float) f * (float) (numSamples - 1) / (float) kTableFrames;
        const float regionEnd = juce::jmin((float) numSamples - 1.f, regionStart + (float) regionSize);

        for (int i = 0; i < kFrameSamples; ++i)
        {
            const float srcPos = regionStart + (regionEnd - regionStart) * (float) i / (float) (kFrameSamples - 1);
            const int i0 = juce::jlimit(0, numSamples - 1, (int) srcPos);
            const int i1 = juce::jlimit(0, numSamples - 1, i0 + 1);
            const float frac = srcPos - (float) i0;
            frame[(size_t) i] = samples[i0] + (samples[i1] - samples[i0]) * frac;
        }

        float dc = 0.f;
        for (float s : frame)
            dc += s;
        dc /= (float) kFrameSamples;

        const float endDelta = frame[(size_t) kFrameSamples - 1] - frame[0];
        for (int i = 0; i < kFrameSamples; ++i)
        {
            const float t = (float) i / (float) (kFrameSamples - 1);
            source[(size_t) frameOffset + (size_t) i] = juce::jlimit(-1.f, 1.f, frame[(size_t) i] - dc - endDelta * t);
        }
    }
}

std::pair<float, float> GranularOsc::processSample()
{
    const float freezeAmount = juce::jlimit(0.f, 1.f, params.freeze);
    if (freezeAmount < 0.999f)
    {
        const float scanScale = (1.f - freezeAmount) * (1.f - freezeAmount);
        const float scanPerSample = params.scanSpeed * 0.75f * scanScale / (float) sr;
        scanPosition = wrap01(scanPosition + scanPerSample);
    }

    float rate = params.density;
    if (params.triggerMode == TriggerMode::AudioRate)
        rate = juce::jlimit(20.f, 1200.f, frequency * (0.25f + params.density / 1200.f));

    if (params.triggerMode == TriggerMode::Burst)
    {
        if (burstRemaining > 0)
        {
            const float burstRate = juce::jlimit(10.f, 900.f, params.density * 2.f);
            triggerPhase += burstRate / (float) sr;
            if (triggerPhase >= 1.f)
            {
                triggerPhase -= 1.f;
                triggerGrain();
                --burstRemaining;
            }
        }
    }
    else
    {
        const float jitterScale = 1.f + randomBipolar() * params.jitter * 0.65f;
        triggerPhase += rate * juce::jlimit(0.1f, 2.f, jitterScale) / (float) sr;
        while (triggerPhase >= 1.f)
        {
            triggerPhase -= 1.f;
            triggerGrain();
        }
    }

    float outL = 0.f;
    float outR = 0.f;

    for (auto& grain : grains)
    {
        if (!grain.active)
            continue;

        const float phase = (float) grain.age / (float) juce::jmax(1, grain.length);
        const float amp = windowValue(grain.window, phase) * grain.gain;
        const float s = readSource(grain.readPos, grain.loopStart, grain.loopLength) * amp;

        outL += s * grain.panL;
        outR += s * grain.panR;

        grain.readPos += grain.step;
        const float loopStart = (float) grain.loopStart;
        const float loopEnd = loopStart + (float) grain.loopLength;
        while (grain.readPos < loopStart)
            grain.readPos += (float) grain.loopLength;
        while (grain.readPos >= loopEnd)
            grain.readPos -= (float) grain.loopLength;

        ++grain.age;
        if (grain.age >= grain.length)
            grain.active = false;
    }

    const float comp = 1.f / std::sqrt((float) kMaxGrains * 0.12f);
    return processBody(clampFinite(outL * comp), clampFinite(outR * comp));
}

int GranularOsc::getActiveGrainCount() const
{
    int count = 0;
    for (const auto& grain : grains)
        if (grain.active)
            ++count;
    return count;
}

void GranularOsc::generateDefaultSource()
{
    for (int f = 0; f < kTableFrames; ++f)
    {
        const float morph = (float) f / (float) (kTableFrames - 1);
        const float brightness = 0.15f + 0.85f * morph;
        const float formantCentre = 2.0f + 22.0f * morph;
        const float oddBias = 0.35f + 0.55f * std::sin(juce::MathConstants<float>::pi * morph);
        const float drive = 0.95f + 1.8f * morph;
        const int maxHarm = juce::jlimit(3, 40, 4 + juce::roundToInt(34.f * morph));

        for (int i = 0; i < kFrameSamples; ++i)
        {
            const float phase = juce::MathConstants<float>::twoPi * (float) i / (float) kFrameSamples;
            float sample = std::sin(phase);
            float norm = 1.f;

            for (int h = 2; h <= maxHarm; ++h)
            {
                const float hf = (float) h;
                const float rolloff = std::pow(hf, -(1.8f - brightness * 0.95f));
                const float oddWeight = (h & 1) ? 1.f : (1.f - oddBias);
                const float formant = 0.35f + 1.15f * std::exp(-0.5f * std::pow((hf - formantCentre) / 3.2f, 2.f));
                const float amp = rolloff * oddWeight * formant;
                sample += amp * std::sin(phase * hf + morph * hf * 0.13f);
                norm += std::abs(amp);
            }

            source[(size_t) f * (size_t) kFrameSamples + (size_t) i] = std::tanh((sample / norm) * drive);
        }
    }
}

void GranularOsc::triggerGrain()
{
    Grain* target = nullptr;
    for (auto& grain : grains)
    {
        if (!grain.active)
        {
            target = &grain;
            break;
        }
    }

    if (target == nullptr)
    {
        target = &grains[0];
        for (auto& grain : grains)
            if (grain.age > target->age)
                target = &grain;
    }

    const float position = wrap01(scanPosition + wrappedDelta(params.position, scanPosition) * params.freeze);
    const float frameSpray = params.spray * params.spray * (float) kTableFrames * 0.35f;
    const float framePos = wrap01(position + randomBipolar() * frameSpray / (float) kTableFrames);
    const int frameIndex = juce::jlimit(0, kTableFrames - 1,
        juce::roundToInt(framePos * (float) (kTableFrames - 1)));
    const int loopStart = frameIndex * kFrameSamples;
    const int loopLength = kFrameSamples;
    const float sizeJitter = 1.f + randomBipolar() * params.jitter * 0.75f;
    const int length = juce::jlimit(8, (int) (sr * 1.5),
        juce::roundToInt(params.sizeMs * 0.001f * (float) sr * juce::jlimit(0.2f, 2.f, sizeJitter)));

    const float pitchRand = randomBipolar() * params.jitter * 4.f;
    const float pitch = params.pitchSemis + pitchRand;
    const float grainHz = juce::jlimit(1.f, 20000.f,
                                       frequency * gliese::dsp::pitchRatioFromSemitones(pitch));
    float step = grainHz * (float) kFrameSamples / (float) sr;

    bool reverse = random01() < params.reverseProb;
    if (params.reverseProb > 0.49f && params.reverseProb < 0.51f)
    {
        reverse = nextReverse;
        nextReverse = !nextReverse;
    }
    if (reverse)
        step = -step;

    const float pan = juce::jlimit(-1.f, 1.f, randomBipolar() * params.panSpread);
    const float left = std::sqrt(0.5f - 0.5f * pan);
    const float right = std::sqrt(0.5f + 0.5f * pan);

    target->active = true;
    target->loopStart = loopStart;
    target->loopLength = loopLength;
    target->readPos = reverse ? (float) (loopStart + loopLength - 1) : (float) loopStart;
    target->step = step;
    target->age = 0;
    target->length = length;
    target->gain = 0.6f + 0.4f * noteVelocity;
    target->panL = left;
    target->panR = right;
    target->window = params.window;
}

float GranularOsc::readSource(float pos, int loopStart, int loopLength) const
{
    loopStart = juce::jlimit(0, kSourceSamples - 1, loopStart);
    loopLength = juce::jlimit(1, kSourceSamples - loopStart, loopLength);
    const int offset0 = juce::jlimit(0, loopLength - 1, (int) std::floor(pos) - loopStart);
    const int offset1 = (offset0 + 1) % loopLength;
    const int i0 = loopStart + offset0;
    const int i1 = loopStart + offset1;
    const float frac = pos - std::floor(pos);
    return source[(size_t) i0] + (source[(size_t) i1] - source[(size_t) i0]) * frac;
}

float GranularOsc::windowValue(Window window, float phase) const
{
    phase = juce::jlimit(0.f, 1.f, phase);

    switch (window)
    {
        case Window::Hann:
            return 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
        case Window::Triangle:
            return 1.f - std::abs(phase * 2.f - 1.f);
        case Window::Gaussian:
        {
            const float x = (phase - 0.5f) * 5.5f;
            return std::exp(-0.5f * x * x);
        }
        case Window::Tukey:
        {
            constexpr float a = 0.45f;
            if (phase < a * 0.5f)
                return 0.5f * (1.f + std::cos(juce::MathConstants<float>::pi * (2.f * phase / a - 1.f)));
            if (phase > 1.f - a * 0.5f)
                return 0.5f * (1.f + std::cos(juce::MathConstants<float>::pi * (2.f * phase / a - 2.f / a + 1.f)));
            return 1.f;
        }
        case Window::Rectangular:
            return 1.f;
        case Window::ExpAttack:
            return std::pow(phase, 2.5f);
        case Window::ExpDecay:
            return std::pow(1.f - phase, 2.5f);
        case Window::NumWindows:
            break;
    }

    return 1.f;
}

void GranularOsc::configureBody()
{
    bodyDirty = false;

    for (auto& mode : bodyModes)
    {
        mode.b0 = mode.b1 = mode.b2 = 0.f;
        mode.a1 = mode.a2 = 0.f;
        mode.gainL = mode.gainR = 0.f;
    }

    if (params.bodyType == BodyType::Off || params.bodyMix <= 0.0001f)
        return;

    const int count = juce::jlimit(4, kMaxBodyModes, params.bodyModes);
    const float sizeShift = (0.5f - params.bodySize) * 2.2f;
    const float rootHz = juce::jlimit(20.f, (float) sr * 0.22f,
        frequency * gliese::dsp::pitchRatioFromSemitones(params.bodyRootSemis + sizeShift * 12.f));
    const float qBase = 3.f + params.bodyDecay * 80.f;
    const float damping = 0.2f + params.bodyDamping * 2.8f;
    const float spread = params.bodySpread;

    for (int i = 0; i < count; ++i)
    {
        auto& mode = bodyModes[(size_t) i];
        const float ratio = modalRatio(params.bodyType, i);
        const float inharm = 1.f + params.bodyInharm * 0.018f * (float) i * (float) i;
        const float hz = rootHz * ratio * inharm;

        if (hz >= (float) sr * 0.47f)
            continue;

        const float omega = juce::MathConstants<float>::twoPi * hz / (float) sr;
        const float sn = std::sin(omega);
        const float cs = std::cos(omega);
        const float modeDamping = damping * (1.f + (float) i * 0.08f);
        const float q = juce::jlimit(0.7f, 120.f, qBase / modeDamping);
        const float alpha = sn / (2.f * q);
        const float a0 = 1.f + alpha;

        mode.b0 = alpha / a0;
        mode.b1 = 0.f;
        mode.b2 = -alpha / a0;
        mode.a1 = (-2.f * cs) / a0;
        mode.a2 = (1.f - alpha) / a0;

        const float brightTilt = std::pow(1.f - (float) i / (float) count, 0.35f + (1.f - params.bodyBrightness) * 3.5f);
        const float baseGain = brightTilt / std::sqrt((float) (i + 1));
        const float pan = ((i & 1) == 0 ? -1.f : 1.f) * spread * ((float) (i + 1) / (float) count);
        mode.gainL = baseGain * std::sqrt(0.5f - 0.5f * pan);
        mode.gainR = baseGain * std::sqrt(0.5f + 0.5f * pan);
    }
}

std::pair<float, float> GranularOsc::processBody(float dryL, float dryR)
{
    if (params.bodyType == BodyType::Off || params.bodyMix <= 0.0001f)
        return { dryL, dryR };

    if (bodyDirty)
        configureBody();

    const int count = juce::jlimit(4, kMaxBodyModes, params.bodyModes);
    const float excite = 0.1f + params.bodyExcite * 1.9f;
    float bodyL = 0.f;
    float bodyR = 0.f;

    for (int i = 0; i < count; ++i)
    {
        auto& mode = bodyModes[(size_t) i];
        if (mode.gainL == 0.f && mode.gainR == 0.f)
            continue;

        const float xL = dryL * excite;
        const float yL = mode.b0 * xL + mode.z1L;
        mode.z1L = mode.b1 * xL - mode.a1 * yL + mode.z2L;
        mode.z2L = mode.b2 * xL - mode.a2 * yL;

        const float xR = dryR * excite;
        const float yR = mode.b0 * xR + mode.z1R;
        mode.z1R = mode.b1 * xR - mode.a1 * yR + mode.z2R;
        mode.z2R = mode.b2 * xR - mode.a2 * yR;

        bodyL += yL * mode.gainL;
        bodyR += yR * mode.gainR;
    }

    const float norm = 1.f / std::sqrt((float) count);
    bodyL = std::tanh(bodyL * norm * 3.f);
    bodyR = std::tanh(bodyR * norm * 3.f);

    const float mix = params.bodyMix;
    return {
        clampFinite(dryL + (bodyL - dryL) * mix),
        clampFinite(dryR + (bodyR - dryR) * mix)
    };
}

float GranularOsc::modalRatio(BodyType type, int index) const
{
    static constexpr float glass[]  { 1.00f, 2.31f, 3.76f, 5.43f, 7.08f, 9.22f, 12.6f, 16.9f };
    static constexpr float metal[]  { 1.00f, 1.59f, 2.14f, 2.92f, 3.89f, 5.41f, 7.12f, 9.53f };
    static constexpr float wood[]   { 1.00f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 24.82f, 31.89f };
    static constexpr float plate[]  { 1.00f, 1.42f, 2.07f, 2.81f, 3.55f, 4.68f, 5.92f, 7.31f };
    static constexpr float drum[]   { 1.00f, 1.63f, 2.14f, 2.30f, 2.65f, 2.92f, 3.16f, 3.60f };
    static constexpr float pipe[]   { 1.00f, 3.00f, 5.00f, 7.00f, 9.00f, 11.0f, 13.0f, 15.0f };
    static constexpr float alien[]  { 1.00f, 1.27f, 1.91f, 2.73f, 4.19f, 6.08f, 9.77f, 14.2f };

    const int i = index % 8;
    const int octave = index / 8;
    const float oct = gliese::dsp::pitchRatioFromSemitones((float) octave * 12.f);

    switch (type)
    {
        case BodyType::Glass:  return glass[i] * oct;
        case BodyType::Metal:  return metal[i] * oct;
        case BodyType::Wood:   return wood[i] * oct;
        case BodyType::Plate:  return plate[i] * oct;
        case BodyType::String: return (float) (index + 1);
        case BodyType::Drum:   return drum[i] * oct;
        case BodyType::Pipe:   return pipe[i] * oct;
        case BodyType::Alien:  return alien[i] * oct;
        case BodyType::Off:
        case BodyType::NumBodyTypes:
            break;
    }

    return (float) (index + 1);
}

float GranularOsc::random01()
{
    rngState = rngState * 1664525u + 1013904223u;
    return (float) ((rngState >> 8u) & 0x00ffffffu) / (float) 0x01000000u;
}

float GranularOsc::randomBipolar()
{
    return random01() * 2.f - 1.f;
}
