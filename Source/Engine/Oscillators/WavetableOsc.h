#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

class WavetableOsc
{
public:
    static constexpr int kTableSize   = 2048;
    static constexpr int kNumMipLevels = 10;   // one per octave band
    static constexpr int kMaxUnison    = 8;

    enum class WarpMode : int
    {
        Off = 0,
        BendPlus,
        BendMinus,
        BendBoth,
        Mirror,
        Sync,
        WindowSync,
        Quantize,
        Fold,
        Formant,
        PDSaw,
        PDSquare,
        PDPulse,
        PDResonant,
        PDFormant,
        NumModes
    };

    struct Table
    {
        float data[kNumMipLevels][kTableSize] = {};
        juce::String name;
    };

    struct Params
    {
        WarpMode warpMode = WarpMode::Off;
        float warpAmount = 0.f;
        int   unison = 1;
        float detune = 0.f;
        float spread = 0.f;
        float positionSpread = 0.f;
        float phaseOffset = 0.f;
        float randomPhase = 0.f;
        float pdBreakpoint = 0.75f;
        bool  retrigger = true;
    };

    void prepare(double sampleRate);
    void setFrequency(float hz);
    void setParams(const Params& params);
    void noteOn();

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
    float  phaseInc = 440.f / 48000.f;
    std::array<float, kMaxUnison> phases {};
    int    tableA = 0;
    int    tableB = 1;
    int    mipLevel = 0;
    float  morphT = 0.f;   // 0 = table A, 1 = table B
    float  tableScanPosition = 0.f;
    Params params;
    juce::uint32 rngState = 0x6d2b79f5u;

    std::vector<Table> tables;

    int selectMipLevel(float frequency) const;
    float cubicInterp(const float* table, float position) const;
    float sampleAt(float phase, float scanOffset) const;
    float warpPhase(float phase) const;
    float warpSample(float sample) const;
    float warpWindow(float carrierPhase, float warpedPhase) const;
    float nextRandom01() noexcept;

    static void buildMipLevel(const float* src, int srcSize,
                               float* dst, int dstSize, int harmonicsLimit);
    static float wrap01(float value) noexcept;
    static float phaseDistort(float phase, float breakpoint) noexcept;
};
