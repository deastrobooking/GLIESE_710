#pragma once
#include <JuceHeader.h>
#include "SynthVoice.h"
#include "Arpeggiator/Arpeggiator.h"
#include <array>

class SynthEngine
{
public:
    static constexpr int kNumVoices = 16;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void setAPVTS(juce::AudioProcessorValueTreeState* apvts);

    void renderNextBlock(juce::AudioBuffer<float>& buffer,
                         juce::MidiBuffer& midiMessages,
                         int startSample,
                         int numSamples);

    void addWavetable(juce::String name, const float* samples, int numSamples);

private:
    juce::AudioProcessorValueTreeState* apvts = nullptr;
    double sampleRate = 48000.0;

    std::array<SynthVoice, kNumVoices> voices;
    Arpeggiator arp;
    juce::MidiBuffer arpMidi;

    void handleMidi(const juce::MidiBuffer& midi, int startSample, int endSample);
    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();

    SynthVoice* findFreeVoice() const;
    SynthVoice* stealVoice() const;
};
