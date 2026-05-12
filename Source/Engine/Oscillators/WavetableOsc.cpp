#include "WavetableOsc.h"

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

//==============================================================================
// WavetableOsc
//==============================================================================

void WavetableOsc::prepare(double sampleRate)
{
    sr = sampleRate;
    if (tables.empty()) buildStandardBank();
    reset();
}

void WavetableOsc::reset()
{
    phase = 0.f;
}

void WavetableOsc::setFrequency(float hz)
{
    freq = juce::jlimit(20.f, (float)(sr * 0.49), hz);
}

void WavetableOsc::setTablePosition(int tableIndex, float position)
{
    int numTables = (int)tables.size();
    if (numTables == 0) return;

    int baseTable = juce::jlimit(0, numTables - 1, tableIndex);
    float scaledPos = (float)baseTable + juce::jlimit(0.f, 1.f, position);
    scaledPos = juce::jlimit(0.f, (float)(numTables - 1), scaledPos);
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
// 8 single-cycle waveforms that span the classic synthesis palette.
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
        buf[i] = std::max(0.f, std::sin(juce::MathConstants<float>::twoPi * (float)i / (float)kTableSize));
    addTable("Half Sine", buf.data(), kTableSize);

    // 6 — Full-rectified sine |sin|: buzzy, reed-like; only even harmonics
    for (int i = 0; i < kTableSize; ++i)
        buf[i] = std::abs(std::sin(juce::MathConstants<float>::twoPi * (float)i / (float)kTableSize));
    addTable("Abs Sine", buf.data(), kTableSize);

    // 7 — Sine³: sub-heavy, smooth saturation character
    for (int i = 0; i < kTableSize; ++i)
    {
        float s = std::sin(juce::MathConstants<float>::twoPi * (float)i / (float)kTableSize);
        buf[i] = s * s * s;
    }
    addTable("Sine Cube", buf.data(), kTableSize);
}

int WavetableOsc::addTable(juce::String name, const float* samples, int numSamples)
{
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

    // JUCE's FFT does not normalise — divide by N to recover original scale
    const float norm = 1.f / (float)N;
    for (int i = 0; i < N; ++i)
        dst[i] = spectrum[(size_t)i].real() * norm;
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

std::pair<float, float> WavetableOsc::processSample()
{
    if (tables.empty()) return { 0.f, 0.f };

    float phaseInc = freq / (float)sr;
    float pos      = phase * (float)kTableSize;
    int   mip      = selectMipLevel(freq);

    float sA = cubicInterp(tables[(size_t)tableA].data[mip], pos);
    float sB = (tableB != tableA) ? cubicInterp(tables[(size_t)tableB].data[mip], pos) : sA;
    float s  = sA + morphT * (sB - sA);

    phase += phaseInc;
    if (phase >= 1.f) phase -= 1.f;

    return { s, s };
}
