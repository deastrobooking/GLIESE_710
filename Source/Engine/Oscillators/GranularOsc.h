#pragma once

#include <JuceHeader.h>
#include <array>

class GranularOsc
{
public:
    static constexpr int kMaxGrains = 96;
    static constexpr int kSourceSamples = 65536;

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
        float pitchSemis = 0.f;     // -48..48
        float spray = 0.18f;        // random position radius
        float jitter = 0.04f;       // trigger timing randomization
        float panSpread = 0.7f;     // stereo randomization
        float reverseProb = 0.f;    // 0..1
        float scanSpeed = 0.f;      // -1..1 sample scan motion
        float freeze = 0.f;         // 0..1

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

    void prepare(double sampleRate);
    void reset();
    void noteOn(int midiNote, float velocity);
    void noteOff();

    void setFrequency(float hz);
    void setParams(const Params& newParams);
    void setSample(const float* samples, int numSamples);

    std::pair<float, float> processSample();
    int getActiveGrainCount() const;

private:
    static constexpr int kMaxBodyModes = 32;

    struct Grain
    {
        bool active = false;
        float readPos = 0.f;
        float step = 1.f;
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

    std::array<float, kSourceSamples> source {};
    std::array<Grain, kMaxGrains> grains {};
    std::array<ModalMode, kMaxBodyModes> bodyModes {};

    uint32_t rngState = 0x6d2b79f5u;
    float triggerPhase = 0.f;
    float scanPosition = 0.5f;
    int burstRemaining = 0;
    bool nextReverse = false;
    bool bodyDirty = true;

    void generateDefaultSource();
    void triggerGrain();
    float readSource(float pos) const;
    float windowValue(Window window, float phase) const;
    void configureBody();
    std::pair<float, float> processBody(float dryL, float dryR);
    float modalRatio(BodyType type, int index) const;
    float random01();
    float randomBipolar();
};
