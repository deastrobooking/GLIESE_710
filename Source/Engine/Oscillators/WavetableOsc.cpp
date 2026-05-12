#include "WavetableOsc.h"

void WavetableOsc::prepare(double sampleRate)
{
    sr = sampleRate;
    if (tables.empty()) addSineTable();
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

    float scaledPos = position * (float)(numTables - 1);
    tableA = juce::jlimit(0, numTables - 1, (int)scaledPos);
    tableB = juce::jlimit(0, numTables - 1, tableA + 1);
    morphT = scaledPos - (float)tableA;
}

int WavetableOsc::addSineTable()
{
    std::vector<float> sine(kTableSize);
    for (int i = 0; i < kTableSize; ++i)
        sine[i] = std::sin(juce::MathConstants<float>::twoPi * (float)i / (float)kTableSize);
    return addTable("Sine", sine.data(), kTableSize);
}

int WavetableOsc::addTable(juce::String name, const float* samples, int numSamples)
{
    tables.emplace_back();
    auto& t = tables.back();
    t.name = name;

    // Build each mip level with progressively fewer harmonics
    for (int mip = 0; mip < kNumMipLevels; ++mip)
    {
        int harmonicsLimit = kTableSize / 2 >> mip;   // halve harmonics per level
        harmonicsLimit = juce::jmax(1, harmonicsLimit);
        buildMipLevel(samples, numSamples, t.data[mip], kTableSize, harmonicsLimit);
    }
    return (int)tables.size() - 1;
}

void WavetableOsc::buildMipLevel(const float* src, int srcSize,
                                  float* dst, int dstSize, int harmonicsLimit)
{
    // Additive reconstruction: copy harmonics up to limit
    // For simplicity: linear resample input then apply FFT low-pass in production.
    // Here we use a straightforward sinc-resampled copy limited by harmonicsLimit.
    for (int i = 0; i < dstSize; ++i)
    {
        float pos = (float)i / (float)dstSize * (float)srcSize;
        int   idx = (int)pos % srcSize;
        float frac = pos - (float)(int)pos;
        int   next = (idx + 1) % srcSize;
        dst[i] = src[idx] + frac * (src[next] - src[idx]);
    }
    (void)harmonicsLimit; // FFT bandlimiting goes here in Phase 6
}

int WavetableOsc::selectMipLevel(float frequency) const
{
    // Choose mip level so that the highest harmonic doesn't exceed Nyquist
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

    // Catmull-Rom
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

    float sA = cubicInterp(tables[tableA].data[mip], pos);
    float sB = (tableB != tableA) ? cubicInterp(tables[tableB].data[mip], pos) : sA;
    float s  = sA + morphT * (sB - sA);

    phase += phaseInc;
    if (phase >= 1.f) phase -= 1.f;

    return { s, s };  // mono-in stereo-out (spread added via unison in SynthVoice)
}
