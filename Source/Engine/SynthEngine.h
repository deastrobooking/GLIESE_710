#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"
#include "Arpeggiator/Arpeggiator.h"
#include "Modulation/StepSequencerMod.h"
#include "MPEZone.h"
#include <array>
#include <optional>

class SynthEngine
{
public:
    static constexpr int kNumVoices = 10;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void setAPVTS(juce::AudioProcessorValueTreeState* apvts);
    void setHostBPM(double bpm);
    void setHostPosition(std::optional<double> ppqPosition);
    int getModSequencerCurrentStep() const noexcept { return modSequencer.getCurrentStep(); }
    int getActiveVoiceCount() const noexcept;

    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         juce::MidiBuffer& midiMessages,
                         int startSample,
                         int numSamples);

    void addWavetable(juce::String name, const float* samples, int numSamples);
    bool setTerrainHeightmap(int index, const juce::Image& image);

private:
    juce::AudioProcessorValueTreeState* apvts = nullptr;
    double sampleRate = 48000.0;

    struct ArpParamCache
    {
        std::atomic<float>* on      = nullptr;
        std::atomic<float>* pattern = nullptr;
        std::atomic<float>* rate    = nullptr;
        std::atomic<float>* sync    = nullptr;
        std::atomic<float>* octaves = nullptr;
        std::atomic<float>* gate    = nullptr;
        std::atomic<float>* swing   = nullptr;
        std::atomic<float>* latch   = nullptr;
        bool built = false;
    };
    ArpParamCache arpParams;

    struct MasterFxCache
    {
        std::atomic<float>* age      = nullptr;
        std::atomic<float>* feedback = nullptr;
        bool built = false;
    };
    MasterFxCache fxParams;

    // Age (bitcrusher) state
    float ageSampleL = 0.f, ageSampleR = 0.f;
    int   ageCounter = 0;
    // Feedback state (block-delay feedback loop)
    float feedbackL  = 0.f, feedbackR  = 0.f;

    void processMasterEffects(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    std::array<SynthVoice, kNumVoices> voices;
    StepSequencerMod modSequencer;
    Arpeggiator arp;
    juce::MidiBuffer arpMidi;
    juce::MidiBuffer renderMidi;
    std::array<std::array<uint8_t, 128>, 16> heldNoteCounts {};
    std::array<bool, 16> sustainDown {};
    bool mpeMasterSustainDown = false;
    GlieseMPEZone mpeZone;

    void handleMidiMessage(const juce::MidiMessage& msg, bool triggerNotes);
    void renderVoices(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void renderTimedMidi(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi,
                         int startSample, int numSamples, bool triggerNotes);
    void noteOn(int note, float velocity, int channel);
    void noteOff(int note, int channel);
    void setSustain(int channel, bool down);
    bool isSustainActive(int channel) const noexcept;
    void setExpression(ModSource source, float value, int sourceChannel);
    void allNotesOff();

    SynthVoice* findFreeVoice() const;
    SynthVoice* stealVoice() const;
    void buildParamCache();
};
