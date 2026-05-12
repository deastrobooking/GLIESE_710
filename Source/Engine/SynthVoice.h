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

    // ── Parameter pointer cache ───────────────────────────────────────────────
    // Caching std::atomic<float>* from APVTS eliminates string construction and
    // hash-map lookups on the audio thread. Built once on the first renderBlock.
    struct ParamCache
    {
        struct OpPtrs {
            std::atomic<float>* coarse   = nullptr;
            std::atomic<float>* fine     = nullptr;
            std::atomic<float>* level    = nullptr;
            std::atomic<float>* feedback = nullptr;
            std::atomic<float>* fixed    = nullptr;
            std::atomic<float>* wave     = nullptr;
            std::atomic<float>* vel      = nullptr;
            std::atomic<float>* key      = nullptr;
            std::atomic<float>* adsrA    = nullptr;
            std::atomic<float>* adsrD    = nullptr;
            std::atomic<float>* adsrS    = nullptr;
            std::atomic<float>* adsrR    = nullptr;
        };
        struct LFOPtrs {
            std::atomic<float>* shape    = nullptr;
            std::atomic<float>* rate     = nullptr;
            std::atomic<float>* sync     = nullptr;
            std::atomic<float>* amount   = nullptr;
            std::atomic<float>* phase    = nullptr;
            std::atomic<float>* retrig   = nullptr;
        };
        struct ModSlotPtrs {
            std::atomic<float>* source   = nullptr;
            std::atomic<float>* dest     = nullptr;
            std::atomic<float>* amount   = nullptr;
            std::atomic<float>* bipolar  = nullptr;
        };

        std::array<OpPtrs,       10> ops;
        std::array<LFOPtrs,       4> lfos;
        std::array<ModSlotPtrs,  32> mods;
        std::array<std::atomic<float>*, 8> macros {};

        // Engine enable flags
        std::atomic<float>* en3D = nullptr;
        std::atomic<float>* enFM = nullptr;
        std::atomic<float>* enWT = nullptr;
        std::atomic<float>* enVA = nullptr;

        bool built = false;
    };
    ParamCache pc;

    // Read all APVTS params and configure sub-engines
    void syncParams(juce::AudioProcessorValueTreeState& apvts);
    void buildModSources();
    void buildParamCache(juce::AudioProcessorValueTreeState& apvts);

    static float midiToHz(int note, float pitchBendSemitones = 0.f);
};
