#include "WavetableOsc.h"
#include "../DSPUtils.h"

//==============================================================================
// Static waveform generators (time-domain, full kTableSize resolution)
//==============================================================================

static void fillSine(float* buf, int N)
{
    for (int i = 0; i < N; ++i)
        buf[i] = std::sin(juce::MathConstants<float>::twoPi * (float)i / (float)N);
}

static void fillTriangle(float* buf, int N)
{
    for (int i = 0; i < N; ++i)
    {
        float t = (float)i / (float)N;
        buf[i] = 1.f - 4.f * std::abs(t - 0.5f);
    }
}

static void fillSaw(float* buf, int N)
{
    for (int i = 0; i < N; ++i)
        buf[i] = 2.f * (float)i / (float)N - 1.f;
}

static void fillSquare(float* buf, int N)
{
    for (int i = 0; i < N; ++i)
        buf[i] = i < N / 2 ? 1.f : -1.f;
}

static void fillPulse(float* buf, int N, float duty)
{
    int thresh = juce::jlimit(1, N - 1, (int)(duty * (float)N));
    for (int i = 0; i < N; ++i)
        buf[i] = i < thresh ? 1.f : -1.f;
}

static void normaliseTable(float* buf, int N)
{
    float peak = 0.f;
    for (int i = 0; i < N; ++i)
        peak = std::max(peak, std::abs(buf[i]));

    if (peak > 1.0e-6f)
        for (int i = 0; i < N; ++i)
            buf[i] /= peak;
}

static void fillAdditive(float* buf, int N, const std::initializer_list<std::pair<float, float>>& partials)
{
    std::fill(buf, buf + N, 0.f);
    for (auto [harmonic, level] : partials)
    {
        for (int i = 0; i < N; ++i)
        {
            float ph = (float)i / (float)N;
            buf[i] += level * std::sin(juce::MathConstants<float>::twoPi * harmonic * ph);
        }
    }
    normaliseTable(buf, N);
}

//==============================================================================
// WavetableOsc
//==============================================================================

void WavetableOsc::prepare(double sampleRate)
{
    sr = sampleRate;
    if (tables.empty()) buildStandardBank();
    setFrequency(freq);
    reset();
}

void WavetableOsc::reset()
{
    for (auto& phase : phases)
        phase = wrap01(params.phaseOffset);
}

void WavetableOsc::noteOn()
{
    if (!params.retrigger)
        return;

    for (auto& phase : phases)
        phase = wrap01(params.phaseOffset + nextRandom01() * params.randomPhase);
}

void WavetableOsc::setFrequency(float hz)
{
    freq = juce::jlimit(20.f, (float)(sr * 0.49), hz);
    phaseInc = freq / (float)sr;
    mipLevel = selectMipLevel(freq);
    rebuildUnisonState();
}

void WavetableOsc::setParams(const Params& p)
{
    params = p;
    params.warpMode = static_cast<WarpMode>(juce::jlimit(0, (int)WarpMode::NumModes - 1,
                                                         (int)params.warpMode));
    params.warpAmount    = juce::jlimit(0.f, 1.f, params.warpAmount);
    params.unison        = juce::jlimit(1, kMaxUnison, params.unison);
    params.detune        = juce::jlimit(0.f, 1.f, params.detune);
    params.spread        = juce::jlimit(0.f, 1.f, params.spread);
    params.positionSpread = juce::jlimit(0.f, 1.f, params.positionSpread);
    params.phaseOffset   = wrap01(params.phaseOffset);
    params.randomPhase   = juce::jlimit(0.f, 1.f, params.randomPhase);
    params.pdBreakpoint  = juce::jlimit(0.f, 1.f, params.pdBreakpoint);
    rebuildUnisonState();
}

void WavetableOsc::rebuildUnisonState()
{
    const int n = juce::jlimit(1, kMaxUnison, params.unison);

    for (int i = 0; i < kMaxUnison; ++i)
    {
        const float pos = n > 1 ? ((float)i / (float)(n - 1)) * 2.f - 1.f : 0.f;
        unisonIncs[(size_t) i] = phaseInc * gliese::dsp::pitchRatioFromCents(pos * params.detune * 100.f);
        unisonScanOffsets[(size_t) i] = pos * params.positionSpread;

        if (n == 1)
        {
            unisonPanL[(size_t) i] = 1.f;
            unisonPanR[(size_t) i] = 1.f;
        }
        else
        {
            const float pan = juce::jlimit(0.f, 1.f, 0.5f + pos * params.spread * 0.5f);
            unisonPanL[(size_t) i] = std::sqrt(1.f - pan) * juce::MathConstants<float>::sqrt2;
            unisonPanR[(size_t) i] = std::sqrt(pan) * juce::MathConstants<float>::sqrt2;
        }
    }

    unisonNorm = n > 1 ? 1.f / std::sqrt((float)n) : 1.f;
}

void WavetableOsc::setTablePosition(int tableIndex, float position)
{
    int numTables = (int)tables.size();
    if (numTables == 0) return;

    int baseTable = juce::jlimit(0, numTables - 1, tableIndex);
    float scaledPos = (float)baseTable + juce::jlimit(0.f, 1.f, position);
    scaledPos = juce::jlimit(0.f, (float)(numTables - 1), scaledPos);
    tableScanPosition = scaledPos;
    tableA = juce::jlimit(0, numTables - 1, (int)scaledPos);
    tableB = juce::jlimit(0, numTables - 1, tableA + 1);
    morphT = scaledPos - (float)tableA;
}

juce::String WavetableOsc::getTableName(int idx) const
{
    if (idx < 0 || idx >= (int)tables.size()) return {};
    return tables[(size_t)idx].name;
}

int WavetableOsc::addSineTable()
{
    std::vector<float> buf((size_t)kTableSize);
    fillSine(buf.data(), kTableSize);
    return addTable("Sine", buf.data(), kTableSize);
}

//==============================================================================
// Built-in waveform bank
// A compact factory bank for wavetable scanning and real-time warp modes.
// The WT TABLE parameter selects which one, and WT POS morphs to the next.
//==============================================================================
void WavetableOsc::buildStandardBank()
{
    tables.clear();
    std::vector<float> buf((size_t)kTableSize);

    // 0 — Sine
    fillSine(buf.data(), kTableSize);
    addTable("Sine", buf.data(), kTableSize);

    // 1 — Triangle
    fillTriangle(buf.data(), kTableSize);
    addTable("Triangle", buf.data(), kTableSize);

    // 2 — Sawtooth
    fillSaw(buf.data(), kTableSize);
    addTable("Saw", buf.data(), kTableSize);

    // 3 — Square (50% duty)
    fillSquare(buf.data(), kTableSize);
    addTable("Square", buf.data(), kTableSize);

    // 4 — Pulse (25% duty)
    fillPulse(buf.data(), kTableSize, 0.25f);
    addTable("Pulse", buf.data(), kTableSize);

    // 5 — Half-rectified sine: soft, nasal character from even harmonics
    for (int i = 0; i < kTableSize; ++i)
        buf[(size_t)i] = std::max(0.f, std::sin(juce::MathConstants<float>::twoPi * (float)i / (float)kTableSize));
    addTable("Half Sine", buf.data(), kTableSize);

    // 6 — Full-rectified sine |sin|: buzzy, reed-like; only even harmonics
    for (int i = 0; i < kTableSize; ++i)
        buf[(size_t)i] = std::abs(std::sin(juce::MathConstants<float>::twoPi * (float)i / (float)kTableSize));
    addTable("Abs Sine", buf.data(), kTableSize);

    // 7 — Sine³: sub-heavy, smooth saturation character
    for (int i = 0; i < kTableSize; ++i)
    {
        float s = std::sin(juce::MathConstants<float>::twoPi * (float)i / (float)kTableSize);
        buf[(size_t)i] = s * s * s;
    }
    addTable("Sine Cube", buf.data(), kTableSize);

    fillAdditive(buf.data(), kTableSize, {
        { 1.f, 1.0f }, { 2.f, 0.15f }, { 3.f, 0.9f }, { 5.f, 0.6f }, { 7.f, 0.35f }, { 9.f, 0.2f } });
    addTable("Hollow Odd", buf.data(), kTableSize);

    fillAdditive(buf.data(), kTableSize, {
        { 1.f, 0.65f }, { 2.f, 1.0f }, { 4.f, 0.55f }, { 8.f, 0.32f }, { 12.f, 0.18f } });
    addTable("Even Glass", buf.data(), kTableSize);

    fillAdditive(buf.data(), kTableSize, {
        { 1.f, 0.45f }, { 2.f, 1.0f }, { 3.f, 0.35f }, { 5.f, 0.8f }, { 8.f, 0.25f }, { 13.f, 0.45f } });
    addTable("Vocal A", buf.data(), kTableSize);

    fillAdditive(buf.data(), kTableSize, {
        { 1.f, 0.35f }, { 3.f, 0.28f }, { 7.f, 1.0f }, { 11.f, 0.55f }, { 17.f, 0.32f }, { 23.f, 0.2f } });
    addTable("Metallic", buf.data(), kTableSize);

    for (int i = 0; i < kTableSize; ++i)
    {
        float ph = (float)i / (float)kTableSize;
        float syncPh = std::fmod(ph * 3.0f, 1.f);
        buf[(size_t)i] = 2.f * syncPh - 1.f;
    }
    normaliseTable(buf.data(), kTableSize);
    addTable("Sync Saw", buf.data(), kTableSize);

    for (int i = 0; i < kTableSize; ++i)
    {
        float ph = (float)i / (float)kTableSize;
        float carrier = std::sin(juce::MathConstants<float>::twoPi * ph);
        float shaper  = std::sin(juce::MathConstants<float>::twoPi * (ph * 6.f + 0.18f * carrier));
        buf[(size_t)i] = std::tanh(2.4f * carrier + 0.9f * shaper);
    }
    normaliseTable(buf.data(), kTableSize);
    addTable("Reso Fold", buf.data(), kTableSize);

    juce::Random noise(0x710710);
    for (int i = 0; i < kTableSize; ++i)
    {
        float ph = (float)i / (float)kTableSize;
        float n  = noise.nextFloat() * 2.f - 1.f;
        buf[(size_t)i] = 0.55f * n + 0.45f * std::sin(juce::MathConstants<float>::twoPi * 17.f * ph);
    }
    normaliseTable(buf.data(), kTableSize);
    addTable("Noisy Digital", buf.data(), kTableSize);
}

int WavetableOsc::addTable(juce::String name, const float* samples, int numSamples)
{
    if (samples == nullptr || numSamples < 2)
        return -1;

    tables.emplace_back();
    auto& t = tables.back();
    t.name = name;

    for (int mip = 0; mip < kNumMipLevels; ++mip)
    {
        int harmonicsLimit = kTableSize / 2 >> mip;
        harmonicsLimit = juce::jmax(1, harmonicsLimit);
        buildMipLevel(samples, numSamples, t.data[mip], kTableSize, harmonicsLimit);
    }
    return (int)tables.size() - 1;
}

//==============================================================================
// FFT-based bandlimited MIP generation
//
// Algorithm (mirrors Vital's approach):
//   1. Linear-resample the source waveform to kTableSize samples.
//   2. Complex FFT → zero all bins above harmonicsLimit → IFFT.
//   3. Each MIP level halves harmonicsLimit, giving an alias-free table for
//      each octave band that selectMipLevel() maps to at runtime.
//==============================================================================
void WavetableOsc::buildMipLevel(const float* src, int srcSize,
                                  float* dst, int dstSize, int harmonicsLimit)
{
    const int N = dstSize;

    // Step 1 — resample to N samples
    std::vector<float> buf((size_t)N);
    for (int i = 0; i < N; ++i)
    {
        float pos  = (float)i / (float)N * (float)srcSize;
        int   idx  = (int)pos % srcSize;
        float frac = pos - (float)idx;
        int   next = (idx + 1) % srcSize;
        buf[(size_t)i] = src[idx] + frac * (src[next] - src[idx]);
    }

    // Step 2 — FFT bandlimit
    int order = 0;
    while ((1 << order) < N) ++order;
    juce::dsp::FFT fft(order);

    using Cpx = juce::dsp::Complex<float>;
    std::vector<Cpx> spectrum((size_t)N), filtered((size_t)N);

    for (int i = 0; i < N; ++i)
        spectrum[(size_t)i] = { buf[(size_t)i], 0.f };

    fft.perform(spectrum.data(), filtered.data(), false);   // forward

    // Zero bins above harmonicsLimit in both positive and negative halves.
    // For a real signal the spectrum is Hermitian, so both halves must be
    // zeroed symmetrically to keep the IFFT output real.
    for (int bin = harmonicsLimit + 1; bin < N - harmonicsLimit; ++bin)
        filtered[(size_t)bin] = { 0.f, 0.f };

    fft.perform(filtered.data(), spectrum.data(), true);    // inverse

    for (int i = 0; i < N; ++i)
        dst[i] = spectrum[(size_t)i].real();
}

//==============================================================================

int WavetableOsc::selectMipLevel(float frequency) const
{
    float nyquist = (float)(sr * 0.5);
    int mip = 0;
    while (mip < kNumMipLevels - 1 && frequency * (float)(kTableSize / 2 >> mip) > nyquist)
        ++mip;
    return mip;
}

float WavetableOsc::cubicInterp(const float* table, float position) const
{
    int   n0 = (int)position;
    float t  = position - (float)n0;

    auto wrap = [](int idx) { return ((idx % kTableSize) + kTableSize) % kTableSize; };

    float p0 = table[wrap(n0 - 1)];
    float p1 = table[wrap(n0)];
    float p2 = table[wrap(n0 + 1)];
    float p3 = table[wrap(n0 + 2)];

    float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
    float b =        p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    float c = -0.5f * p0            + 0.5f * p2;
    float d = p1;

    return ((a * t + b) * t + c) * t + d;
}

float WavetableOsc::wrap01(float value) noexcept
{
    value -= std::floor(value);
    return value < 0.f ? value + 1.f : value;
}

float WavetableOsc::nextRandom01() noexcept
{
    rngState = rngState * 1664525u + 1013904223u;
    return (float)(rngState >> 8) * (1.0f / 16777216.0f);
}

float WavetableOsc::phaseDistort(float ph, float breakpoint) noexcept
{
    breakpoint = juce::jlimit(0.001f, 0.999f, breakpoint);
    if (ph < breakpoint)
        return 0.5f * ph / breakpoint;

    return 0.5f + 0.5f * (ph - breakpoint) / (1.f - breakpoint);
}

float WavetableOsc::warpPhase(float ph) const
{
    const float a = params.warpAmount;
    if (a <= 1.0e-5f)
        return ph;

    switch (params.warpMode)
    {
        case WarpMode::BendPlus:
            return std::pow(ph, 1.f + a * 8.f);

        case WarpMode::BendMinus:
            return 1.f - std::pow(1.f - ph, 1.f + a * 8.f);

        case WarpMode::BendBoth:
        {
            float power = 1.f + a * 7.f;
            return ph < 0.5f
                ? 0.5f * std::pow(ph * 2.f, power)
                : 1.f - 0.5f * std::pow((1.f - ph) * 2.f, power);
        }

        case WarpMode::Mirror:
        {
            float mirrored = ph < 0.5f ? ph * 2.f : 2.f - ph * 2.f;
            return ph + a * (mirrored - ph);
        }

        case WarpMode::Sync:
        case WarpMode::WindowSync:
        {
            float ratio = 1.f + a * 8.f;
            return wrap01(ph * ratio);
        }

        case WarpMode::Formant:
        {
            float ratio = 1.f + a * 12.f;
            return wrap01(ph * ratio);
        }

        case WarpMode::PDSaw:
        {
            float bp = 0.5f + (params.pdBreakpoint - 0.5f) * a;
            return phaseDistort(ph, bp);
        }

        case WarpMode::PDSquare:
        {
            float bp = 0.5f + (params.pdBreakpoint - 0.5f) * a;
            if (ph < 0.5f)
                return 0.5f * phaseDistort(ph * 2.f, bp);
            return 0.5f + 0.5f * phaseDistort((ph - 0.5f) * 2.f, bp);
        }

        case WarpMode::PDPulse:
        {
            float duty = juce::jlimit(0.05f, 0.95f, 0.5f + (params.pdBreakpoint - 0.5f) * a);
            float bp = juce::jlimit(0.05f, 0.95f, 0.5f + a * 0.45f);
            if (ph < duty)
                return 0.5f * phaseDistort(ph / duty, bp);
            return 0.5f + 0.5f * phaseDistort((ph - duty) / (1.f - duty), 1.f - bp);
        }

        case WarpMode::PDResonant:
        {
            float bp = 0.5f + (params.pdBreakpoint - 0.5f) * a;
            float resonantPhase = phaseDistort(ph, bp);
            return wrap01(resonantPhase * (1.f + a * 15.f));
        }

        case WarpMode::PDFormant:
        {
            float bp = 0.5f + (params.pdBreakpoint - 0.5f) * a;
            float formantPhase = phaseDistort(ph, bp);
            return wrap01(formantPhase * (1.f + a * 7.f));
        }

        case WarpMode::Off:
        case WarpMode::Quantize:
        case WarpMode::Fold:
        case WarpMode::NumModes:
            return ph;
    }

    return ph;
}

float WavetableOsc::warpWindow(float carrierPhase, float warpedPhase) const
{
    const float a = params.warpAmount;
    if (a <= 1.0e-5f)
        return 1.f;

    if (params.warpMode == WarpMode::WindowSync)
    {
        float window = std::sin(juce::MathConstants<float>::pi * carrierPhase);
        return 1.f + a * (window - 1.f);
    }

    if (params.warpMode == WarpMode::Formant)
    {
        float window = std::sin(juce::MathConstants<float>::pi * warpedPhase);
        return 1.f + a * (window * window - 1.f);
    }

    if (params.warpMode == WarpMode::PDResonant)
    {
        float window = std::sin(juce::MathConstants<float>::pi * carrierPhase);
        window *= window;
        return 1.f + a * (window - 1.f);
    }

    if (params.warpMode == WarpMode::PDFormant)
    {
        float window = std::sin(juce::MathConstants<float>::twoPi * carrierPhase);
        window = 0.5f + 0.5f * window;
        return 1.f + a * (window - 1.f);
    }

    return 1.f;
}

float WavetableOsc::warpSample(float sample) const
{
    const float a = params.warpAmount;
    if (a <= 1.0e-5f)
        return sample;

    switch (params.warpMode)
    {
        case WarpMode::Quantize:
        {
            float levels = 64.f + (2.f - 64.f) * a;
            return std::round(sample * levels) / levels;
        }

        case WarpMode::Fold:
        {
            float drive = 1.f + a * 6.f;
            float x = std::fmod(sample * drive + 1.f, 4.f);
            if (x < 0.f) x += 4.f;
            return x <= 2.f ? x - 1.f : 3.f - x;
        }

        case WarpMode::Off:
        case WarpMode::BendPlus:
        case WarpMode::BendMinus:
        case WarpMode::BendBoth:
        case WarpMode::Mirror:
        case WarpMode::Sync:
        case WarpMode::WindowSync:
        case WarpMode::Formant:
        case WarpMode::PDSaw:
        case WarpMode::PDSquare:
        case WarpMode::PDPulse:
        case WarpMode::PDResonant:
        case WarpMode::PDFormant:
        case WarpMode::NumModes:
            return sample;
    }

    return sample;
}

float WavetableOsc::sampleAt(float ph, float scanOffset) const
{
    if (tables.empty()) return 0.f;

    const int numTables = (int)tables.size();
    float scan = juce::jlimit(0.f, (float)(numTables - 1), tableScanPosition + scanOffset);
    int localA = juce::jlimit(0, numTables - 1, (int)scan);
    int localB = juce::jlimit(0, numTables - 1, localA + 1);
    float localT = scan - (float)localA;

    float warped = warpPhase(ph);
    float pos = warped * (float)kTableSize;
    float sA = cubicInterp(tables[(size_t)localA].data[mipLevel], pos);
    float sB = localB != localA ? cubicInterp(tables[(size_t)localB].data[mipLevel], pos) : sA;
    float s  = sA + localT * (sB - sA);

    s *= warpWindow(ph, warped);
    return juce::jlimit(-1.2f, 1.2f, warpSample(s));
}

std::pair<float, float> WavetableOsc::processSample()
{
    if (tables.empty()) return { 0.f, 0.f };

    const int n = juce::jlimit(1, kMaxUnison, params.unison);
    float outL = 0.f;
    float outR = 0.f;

    for (int i = 0; i < n; ++i)
    {
        float s = sampleAt(phases[(size_t)i], unisonScanOffsets[(size_t)i]);

        if (n == 1)
        {
            outL += s;
            outR += s;
        }
        else
        {
            outL += s * unisonPanL[(size_t)i];
            outR += s * unisonPanR[(size_t)i];
        }

        phases[(size_t)i] = wrap01(phases[(size_t)i] + unisonIncs[(size_t)i]);
    }

    return { juce::jlimit(-1.5f, 1.5f, outL * unisonNorm),
             juce::jlimit(-1.5f, 1.5f, outR * unisonNorm) };
}
