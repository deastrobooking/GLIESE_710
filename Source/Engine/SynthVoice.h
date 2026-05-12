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
    static constexpr int kNumVAOscs = 4;

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
    void buildParamCache(juce::AudioProcessorValueTreeState& apvts);

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
    std::array<VAOscillator, kNumVAOscs> vaOscs;

    EnvelopeBank envBank;
    LFOEngine    lfo;
    DualFilter   dualFilter;
    ModMatrix    modMatrix;

    ModValues modSources, modDests;

    // ── Parameter pointer cache ───────────────────────────────────────────────
    // Caching std::atomic<float>* from APVTS eliminates string construction and
    // hash-map lookups on the audio thread. Built during engine setup, with a
    // render-time fallback only for unusual host lifecycle ordering.
    struct ParamCache
    {
        struct EnvPtrs {
            std::atomic<float>* a = nullptr;
            std::atomic<float>* d = nullptr;
            std::atomic<float>* s = nullptr;
            std::atomic<float>* r = nullptr;
        };
        struct MasterPtrs {
            std::atomic<float>* volume = nullptr;
            std::atomic<float>* tune   = nullptr;
            std::atomic<float>* octave = nullptr;
            std::atomic<float>* pan    = nullptr;
            EnvPtrs env;
        };
        struct Osc3DPtrs {
            std::atomic<float>* field     = nullptr;
            std::atomic<float>* path      = nullptr;
            std::atomic<float>* radius    = nullptr;
            std::atomic<float>* depth     = nullptr;
            std::atomic<float>* warp      = nullptr;
            std::atomic<float>* rotation  = nullptr;
            std::atomic<float>* chaos     = nullptr;
            std::atomic<float>* morph     = nullptr;
            std::atomic<float>* fieldFreq = nullptr;
            std::atomic<float>* zRatio    = nullptr;
            std::atomic<float>* fold      = nullptr;
            std::atomic<float>* drive     = nullptr;
            std::atomic<float>* on        = nullptr;
            std::atomic<float>* level     = nullptr;
            std::atomic<float>* octave    = nullptr;
            EnvPtrs env;
        };
        struct FMPtrs {
            std::atomic<float>* on        = nullptr;
            std::atomic<float>* algorithm = nullptr;
            std::atomic<float>* level     = nullptr;
            std::atomic<float>* octave    = nullptr;
            EnvPtrs env;
        };
        struct WTPtrs {
            std::atomic<float>* on     = nullptr;
            std::atomic<float>* table  = nullptr;
            std::atomic<float>* pos    = nullptr;
            std::atomic<float>* level  = nullptr;
            std::atomic<float>* octave = nullptr;
            EnvPtrs env;
        };
        struct VAPtrs {
            std::atomic<float>* on      = nullptr;
            std::atomic<float>* shape   = nullptr;
            std::atomic<float>* pw      = nullptr;
            std::atomic<float>* detune  = nullptr;
            std::atomic<float>* unison  = nullptr;
            std::atomic<float>* spread  = nullptr;
            std::atomic<float>* ring    = nullptr;
            std::atomic<float>* fold    = nullptr;
            std::atomic<float>* level   = nullptr;
            std::atomic<float>* octave  = nullptr;
            EnvPtrs env;
        };
        struct FilterPtrs {
            std::atomic<float>* type      = nullptr;
            std::atomic<float>* cutoff    = nullptr;
            std::atomic<float>* resonance = nullptr;
            std::atomic<float>* drive     = nullptr;
            std::atomic<float>* envAmt    = nullptr;
            std::atomic<float>* lfoAmt    = nullptr;
            EnvPtrs env;
        };
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
        MasterPtrs master;
        Osc3DPtrs osc3d;
        FMPtrs fm;
        WTPtrs wt;
        std::array<VAPtrs, kNumVAOscs> va;
        FilterPtrs filterA;
        FilterPtrs filterB;
        std::atomic<float>* filterRouting = nullptr;

        bool built = false;
    };
    ParamCache pc;

    // Read all APVTS params and configure sub-engines
    void syncParams();
    void buildModSources();

    static float midiToHz(int note, float pitchBendSemitones = 0.f);
};
