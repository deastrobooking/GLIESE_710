#pragma once
#include <JuceHeader.h>
#include <vector>

class WavetableOsc
{
public:
    static constexpr int kTableSize   = 2048;
    static constexpr int kNumMipLevels = 10;   // one per octave band

    struct Table
    {
        float data[kNumMipLevels][kTableSize] = {};
        juce::String name;
    };

    void prepare(double sampleRate);
    void setFrequency(float hz);

    // tableIndex: which table in the bank; position: 0..1 for morphing between adjacent tables
    void setTablePosition(int tableIndex, float position);

    // Add a waveform (single-cycle float buffer, any size) — mip levels auto-built
    int addTable(juce::String name, const float* samples, int numSamples);
    int addSineTable();

    // Populate the oscillator with the built-in waveform bank (called automatically by prepare)
    void buildStandardBank();

    int getNumTables() const { return (int)tables.size(); }
    juce::String getTableName(int idx) const;


    // Returns stereo pair
    std::pair<float, float> processSample();
    void reset();

private:
    double sr     = 48000.0;
    float  freq   = 440.f;
    float  phase  = 0.f;
    int    tableA = 0;
    int    tableB = 1;
    float  morphT = 0.f;   // 0 = table A, 1 = table B

    std::vector<Table> tables;

    int selectMipLevel(float frequency) const;
    float cubicInterp(const float* table, float position) const;

    static void buildMipLevel(const float* src, int srcSize,
                               float* dst, int dstSize, int harmonicsLimit);
};
