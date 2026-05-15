#pragma once
#include <JuceHeader.h>
#include "Oscillators/Osc3D.h"
#include "Oscillators/FMAlgorithm.h"
#include "Oscillators/WavetableOsc.h"
#include "Oscillators/VAOscillator.h"
#include "Oscillators/KarplusStrong.h"
#include "Envelopes/EnvelopeBank.h"
#include "LFO/LFOEngine.h"
#include "Filter/DualFilter.h"
#include "Modulation/ModMatrix.h"

class SynthVoice
{
public:
    static constexpr int kVoiceId   = -1;
    static constexpr int kNumVAOscs = 4;
    static constexpr int kNum3DOscs = 6;

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

    // Portamento state
    float  portaHz       = 0.f;
    float  portaTargetHz = 0.f;

    // MIDI mod source values
    float midiCCs[(int)ModSource::NumSources] = {};

    double sampleRate = 48000.0;

    // Engine state (set in syncParams, read in renderBlock)
    int  current3dCount   = 1;
    bool currentUseDualFM = false;

    std::array<Osc3D, kNum3DOscs> osc3dArray;
    FMAlgorithm  fm;
    FMAlgorithm  fm2;
    WavetableOsc wtOsc;
    std::array<VAOscillator, kNumVAOscs> vaOscs;
    KarplusStrong ksOsc;
    KarplusStrong::Params cachedKSParams;

    EnvelopeBank envBank;
    LFOEngine    lfo;
    DualFilter   dualFilter;
    ModMatrix    modMatrix;

    ModValues modSources, modDests;

    // ── Parameter pointer cache ───────────────────────────────────────────────
    struct ParamCache
    {
        struct EnvPtrs {
            std::atomic<float>* a = nullptr;
            std::atomic<float>* d = nullptr;
            std::atomic<float>* s = nullptr;
            std::atomic<float>* r = nullptr;
        };
        struct MasterPtrs {
            std::atomic<float>* volume      = nullptr;
            std::atomic<float>* tune        = nullptr;
            std::atomic<float>* octave      = nullptr;
            std::atomic<float>* pan         = nullptr;
            std::atomic<float>* portamento  = nullptr;
            std::atomic<float>* pbRange     = nullptr;
            std::atomic<float>* mwRange     = nullptr;
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
        struct KSPtrs {
            std::atomic<float>* on         = nullptr;
            std::atomic<float>* level      = nullptr;
            std::atomic<float>* octave     = nullptr;
            std::atomic<float>* decay      = nullptr;
            std::atomic<float>* brightness = nullptr;
            std::atomic<float>* stiffness  = nullptr;
            std::atomic<float>* morph      = nullptr;
            std::atomic<float>* position   = nullptr;
            EnvPtrs env;
        };
        struct EngineConfigPtrs {
            std::atomic<float>* osc3dCount  = nullptr;
            std::atomic<float>* osc3dDetune = nullptr;
            std::atomic<float>* fmDual      = nullptr;
            std::atomic<float>* fmDetune    = nullptr;
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
            std::atomic<float>* syncRate = nullptr;
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
        MasterPtrs       master;
        Osc3DPtrs        osc3d;
        FMPtrs           fm;
        WTPtrs           wt;
        std::array<VAPtrs, kNumVAOscs> va;
        KSPtrs           ks;
        EngineConfigPtrs engineConfig;
        FilterPtrs       filterA;
        FilterPtrs       filterB;
        std::atomic<float>* filterRouting = nullptr;

        bool built = false;
    };
    ParamCache pc;

    void syncParams();
    void buildModSources();

    static float midiToHz(int note, float pitchBendSemitones = 0.f);
};
