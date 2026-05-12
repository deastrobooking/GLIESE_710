#pragma once
#include <JuceHeader.h>
#include "Oscillators/Osc3D.h"
#include "Oscillators/FMAlgorithm.h"
#include "Oscillators/WavetableOsc.h"
#include "Oscillators/VAOscillator.h"
#include "Envelopes/EnvelopeBank.h"
#include "LFO/LFOEngine.h"
#include "Filter/DualFilter.h"
#include "Modulation/ModMatrix.h"

class SynthVoice
{
public:
    static constexpr int kVoiceId = -1;

    void prepare(double sampleRate);
    void reset();

    void noteOn(int midiNote, float velocity);
    void noteOff();

    bool isActive() const;
    int  getMidiNote() const { return midiNote; }
    int  getVoiceAge() const { return age; }

    // Called each block from SynthEngine — fills outputL/R buffers (accumulate)
    void renderBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                     juce::AudioProcessorValueTreeState& apvts);

    // Route MIDI CC/AT/PB into mod sources (called from message-to-audio bridge)
    void setMidiCC(ModSource src, float value);
    void setHostBPM(double bpm);
    float getModDestination(ModDest dest) const { return modDests.dests[(int)dest]; }

    WavetableOsc& getWavetableOsc() { return wtOsc; }

private:
    int    midiNote = -1;
    float  velocity = 1.f;
    int    age      = 0;

    // MIDI mod source values set from handleMidi — read in buildModSources()
    float midiCCs[(int)ModSource::NumSources] = {};

    double sampleRate = 48000.0;

    Osc3D       osc3d;
    FMAlgorithm fm;
    WavetableOsc wtOsc;
    VAOscillator vaOsc;

    EnvelopeBank envBank;
    LFOEngine    lfo;
    DualFilter   dualFilter;
    ModMatrix    modMatrix;

    ModValues modSources, modDests;

    // Read all APVTS params and configure sub-engines
    void syncParams(juce::AudioProcessorValueTreeState& apvts);
    void buildModSources();

    static float midiToHz(int note, float pitchBendSemitones = 0.f);
};
