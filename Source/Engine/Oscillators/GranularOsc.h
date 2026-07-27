#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>

class GranularOsc
{
public:
    static constexpr int kMaxGrains = 96;
    static constexpr int kSourceSamples = 65536;
    static constexpr int kTableFrames = 128;
    static constexpr int kFrameSamples = kSourceSamples / kTableFrames;
    static constexpr int kMaxTransientMarkers = 32;
    static constexpr int kMaxVisualGrains = 24;
    struct SampleData
    {
        std::array<float, kSourceSamples> samples {};
        std::array<float, kSourceSamples> samplesR {};
        std::array<float, kMaxTransientMarkers + 2> transientMarkers {};
        int transientMarkerCount = 2; // includes 0 and 1 boundaries

        float& operator[](size_t index) noexcept { return samples[index]; }
        const float& operator[](size_t index) const noexcept { return samples[index]; }
        constexpr size_t size() const noexcept { return samples.size(); }
        auto begin() noexcept { return samples.begin(); }
        auto end() noexcept { return samples.end(); }
        auto begin() const noexcept { return samples.begin(); }
        auto end() const noexcept { return samples.end(); }
    };

    enum class TriggerMode
    {
        FreeCloud = 0,
        Burst,
        AudioRate,
        NumModes
    };

    enum class Window
    {
        Hann = 0,
        Triangle,
        Gaussian,
        Tukey,
        Rectangular,
        ExpAttack,
        ExpDecay,
        NumWindows
    };

    enum class BodyType
    {
        Off = 0,
        Glass,
        Metal,
        Wood,
        Plate,
        String,
        Drum,
        Pipe,
        Alien,
        NumBodyTypes
    };

    struct Params
    {
        TriggerMode triggerMode = TriggerMode::FreeCloud;
        Window window = Window::Hann;
        float position = 0.5f;      // 0..1
        float sizeMs = 120.f;       // 5..1000
        float density = 35.f;       // grains per second
        bool densitySync = false;
        int densityDivision = 4;    // quarter-note default
        double hostBPM = 120.0;
        float pitchSemis = 0.f;     // -48..48
        float pitchCloudSemis = 0.f;// random +/- semitone spread
        bool pitchQuantize = false;
        int pitchScale = 0;
        int pitchScaleRoot = 0;
        float spray = 0.18f;        // random position radius
        float jitter = 0.04f;       // trigger timing randomization
        float panSpread = 0.7f;     // stereo randomization
        float reverseProb = 0.f;    // 0..1
        int directionMode = 0;      // probability/forward/reverse/alternate/pattern
        int directionLength = 8;
        uint8_t directionPattern = 0;
        float scanSpeed = 0.f;      // -1..1 sample scan motion
        float freeze = 0.f;         // 0..1
        float regionStart = 0.f;    // normalized source region
        float regionEnd = 1.f;
        int sliceCount = 1;         // equal divisions of the selected region
        int sliceIndex = 0;
        bool transientSlices = false;
        int rootNote = 60;
        float layerMorph = 0.f;

        BodyType bodyType = BodyType::Off;
        float bodyRootSemis = 0.f;  // modal root relative to played pitch
        float bodySize = 0.5f;      // 0..1, larger = lower body
        float bodyDecay = 0.5f;     // 0..1
        float bodyDamping = 0.35f;  // 0..1
        float bodyBrightness = 0.6f;// 0..1
        float bodyInharm = 0.2f;    // 0..1
        float bodyExcite = 0.7f;    // 0..1
        float bodyMix = 0.f;        // 0..1, default keeps pure granular
        float bodySpread = 0.5f;    // 0..1
        int bodyModes = 16;         // 4..32
    };

    struct VisualSnapshot
    {
        int count = 0;
        std::array<float, kMaxVisualGrains> position {};
        std::array<float, kMaxVisualGrains> progress {};
        std::array<float, kMaxVisualGrains> pan {};
        std::array<bool, kMaxVisualGrains> reverse {};
    };

    void prepare(double sampleRate);
    void reset();
    void noteOn(int midiNote, float velocity);
    void noteOff();

    void setFrequency(float hz);
    void setParams(const Params& newParams);
    void setSample(const float* samples, int numSamples);
    void setSampleData(std::shared_ptr<const SampleData> newSample);
    void setLayerSampleData(int layer, std::shared_ptr<const SampleData> newSample);
    static std::shared_ptr<const SampleData> createSampleData(const float* samples, int numSamples);
    static std::shared_ptr<const SampleData> createSampleData(
        const float* left, const float* right, int numSamples);
    static juce::MemoryBlock serializeSampleData(const SampleData& sample);
    static std::shared_ptr<const SampleData> deserializeSampleData(
        const void* data, size_t size);

    std::pair<float, float> processSample();
    int getActiveGrainCount() const;
    VisualSnapshot getVisualSnapshot() const noexcept;
    static float densityRateForTempo(double bpm, int division) noexcept;
    static float quantizePitchOffset(int midiNote, float semitoneOffset,
                                     int scaleRoot, int scale) noexcept;
    static bool directionForGrain(int mode, uint8_t pattern, int length,
                                  int sequenceIndex, float randomValue,
                                  float reverseProbability) noexcept;

private:
    static constexpr int kMaxBodyModes = 32;

    struct Grain
    {
        bool active = false;
        float readPos = 0.f;
        float step = 1.f;
        int loopStart = 0;
        int loopLength = kFrameSamples;
        int age = 0;
        int length = 1;
        float gain = 1.f;
        float panL = 0.7071f;
        float panR = 0.7071f;
        Window window = Window::Hann;
    };

    struct ModalMode
    {
        float b0 = 0.f, b1 = 0.f, b2 = 0.f;
        float a1 = 0.f, a2 = 0.f;
        float z1L = 0.f, z2L = 0.f;
        float z1R = 0.f, z2R = 0.f;
        float gainL = 0.f, gainR = 0.f;
    };

    double sr = 48000.0;
    float frequency = 440.f;
    int note = 60;
    float noteVelocity = 1.f;
    Params params;

    std::shared_ptr<const SampleData> source;
    std::shared_ptr<const SampleData> sourceB;
    std::array<Grain, kMaxGrains> grains {};
    std::array<ModalMode, kMaxBodyModes> bodyModes {};

    uint32_t rngState = 0x6d2b79f5u;
    float triggerPhase = 0.f;
    float scanPosition = 0.5f;
    int burstRemaining = 0;
    int directionCounter = 0;
    int visualPublishCounter = 0;
    std::atomic<int> visualCount { 0 };
    std::array<std::atomic<float>, kMaxVisualGrains> visualPosition {};
    std::array<std::atomic<float>, kMaxVisualGrains> visualProgress {};
    std::array<std::atomic<float>, kMaxVisualGrains> visualPan {};
    std::array<std::atomic<bool>, kMaxVisualGrains> visualReverse {};
    bool bodyDirty = true;

    static std::shared_ptr<const SampleData> generateDefaultSource();
    void triggerGrain();
    void publishVisualSnapshot() noexcept;
    static float readSource(const std::shared_ptr<const SampleData>& sample,
                            float pos, int loopStart, int loopLength,
                            bool rightChannel);
    float windowValue(Window window, float phase) const;
    void configureBody();
    std::pair<float, float> processBody(float dryL, float dryR);
    float modalRatio(BodyType type, int index) const;
    float random01();
    float randomBipolar();
};
