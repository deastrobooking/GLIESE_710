#pragma once
#include <JuceHeader.h>
#include "Oscillators/Osc3D.h"
#include "Oscillators/FMAlgorithm.h"
#include "Oscillators/WavetableOsc.h"
#include "Oscillators/VAOscillator.h"
#include "Oscillators/KarplusStrong.h"
#include "Oscillators/GranularOsc.h"
#include "Oscillators/TerrainOsc.h"
#include "Envelopes/EnvelopeBank.h"
#include "LFO/LFOEngine.h"
#include "Filter/DualFilter.h"
#include "Modulation/ModMatrix.h"

class SynthVoice
{
public:
    static constexpr int kVoiceId      = -1;
    static constexpr int kNum3DOscs    = 8;  // max 3D instances (8 slots, each can be 3D)
    static constexpr int kNumVAOscs    = 8;  // max VA instances
    static constexpr int kNumWTOscs    = 8;  // max WT instances
    static constexpr int kNumKSOscs    = 8;  // max KS instances
    static constexpr int kNumGranOscs  = 8;  // max granular instances
    static constexpr int kNumTerrainOscs = 8; // max terrain instances
    static constexpr int kNumEngineFilters = 7;

    void prepare(double sampleRate);
    void reset();

    void noteOn(int midiNote, float velocity, int midiChannel = 1);
    void noteOff();

    bool isActive() const;
    int  getMidiNote() const { return midiNote; }
    int  getMidiChannel() const { return midiChannel; }
    int  getVoiceAge() const { return age; }

    void renderBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                     juce::AudioProcessorValueTreeState& apvts,
                     const ModValues& globalModDests);

    void setMidiCC(ModSource src, float value);
    void setHostBPM(double bpm);
    float getModDestination(ModDest dest) const { return modDests.dests[(int)dest]; }

    WavetableOsc& getWavetableOsc() { return wtOscs[0]; }
    void addWavetable(juce::String name, const float* samples, int numSamples);
    bool setTerrainHeightmap(int index, const juce::Image& image);
    void shareTerrainHeightmap(int index, const SynthVoice& source);
    void buildParamCache(juce::AudioProcessorValueTreeState& apvts);

private:
    int    midiNote = -1;
    int    midiChannel = 1;
    float  velocity = 1.f;
    int    age      = 0;

    float  portaHz       = 0.f;
    float  portaTargetHz = 0.f;

    float midiCCs[(int)ModSource::NumSources] = {};

    double sampleRate = 48000.0;

    // Active engine counts (derived in syncParams, used in renderBlock)
    int  current3dCount    = 0;
    int  currentVACount    = 0;
    int  currentWTCount    = 0;
    int  currentKSCount    = 0;
    int  currentGranCount  = 0;
    int  currentTerrainCount = 0;
    bool currentBlockAIsFM = false;
    bool currentBlockBIsFM = false;
    std::array<int, kNum3DOscs>   active3dIndices {};
    std::array<int, kNumVAOscs>   activeVAIndices {};
    std::array<int, kNumWTOscs>   activeWTIndices {};
    std::array<int, kNumKSOscs>   activeKSIndices {};
    std::array<int, kNumGranOscs> activeGranIndices {};
    std::array<int, kNumTerrainOscs> activeTerrainIndices {};

    std::array<Osc3D,         kNum3DOscs> osc3dArray;
    FMAlgorithm  fm;
    FMAlgorithm  fm2;
    std::array<WavetableOsc,  kNumWTOscs> wtOscs;
    std::array<VAOscillator,  kNumVAOscs> vaOscs;
    std::array<KarplusStrong, kNumKSOscs> ksOscs;
    std::array<GranularOsc,   kNumGranOscs> granularOscs;
    std::array<TerrainOsc,    kNumTerrainOscs> terrainOscs;
    std::array<KarplusStrong::Params, kNumKSOscs> cachedKSParams;

    EnvelopeBank envBank;
    LFOEngine    lfo;
    std::array<SingleFilter, kNumEngineFilters> engineFilterL;
    std::array<SingleFilter, kNumEngineFilters> engineFilterR;
    DualFilter   dualFilter;
    ModMatrix    modMatrix;

    ModValues modSources, modDests;

    using LinearSmooth = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;
    LinearSmooth masterVolumeSmooth;
    LinearSmooth masterPanSmooth;
    std::array<LinearSmooth, 2> fmLevelSmooth;
    std::array<LinearSmooth, kNum3DOscs> osc3dLevelSmooth;
    std::array<LinearSmooth, kNumWTOscs> wtLevelSmooth;
    std::array<LinearSmooth, kNumKSOscs> ksLevelSmooth;
    std::array<LinearSmooth, kNumGranOscs> grainLevelSmooth;
    std::array<LinearSmooth, kNumTerrainOscs> terrainLevelSmooth;
    std::array<LinearSmooth, kNumVAOscs> vaLevelSmooth;
    bool smoothingInitialized = false;

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
            std::atomic<float>* tune      = nullptr;
            std::atomic<float>* octave    = nullptr;
            EnvPtrs env;
        };
        struct FMPtrs {
            std::atomic<float>* on        = nullptr;
            std::atomic<float>* algorithm = nullptr;
            std::atomic<float>* level     = nullptr;
            std::atomic<float>* tune      = nullptr;
            std::atomic<float>* octave    = nullptr;
            std::atomic<float>* detune    = nullptr;
            std::atomic<float>* phase     = nullptr;
            std::atomic<float>* algoMorph = nullptr;
            std::atomic<float>* indexScale = nullptr;
            EnvPtrs env;
        };
        struct WTPtrs {
            std::atomic<float>* on     = nullptr;
            std::atomic<float>* table  = nullptr;
            std::atomic<float>* pos    = nullptr;
            std::atomic<float>* warpMode = nullptr;
            std::atomic<float>* warp   = nullptr;
            std::atomic<float>* pdBreak = nullptr;
            std::atomic<float>* unison = nullptr;
            std::atomic<float>* detune = nullptr;
            std::atomic<float>* spread = nullptr;
            std::atomic<float>* posSpread = nullptr;
            std::atomic<float>* phase  = nullptr;
            std::atomic<float>* random = nullptr;
            std::atomic<float>* retrig = nullptr;
            std::atomic<float>* level  = nullptr;
            std::atomic<float>* tune   = nullptr;
            std::atomic<float>* octave = nullptr;
            EnvPtrs env;
        };
        struct VAPtrs {
            std::atomic<float>* on      = nullptr;
            std::atomic<float>* model   = nullptr;
            std::atomic<float>* shape   = nullptr;
            std::atomic<float>* pw      = nullptr;
            std::atomic<float>* detune  = nullptr;
            std::atomic<float>* unison  = nullptr;
            std::atomic<float>* spread  = nullptr;
            std::atomic<float>* ring    = nullptr;
            std::atomic<float>* fold    = nullptr;
            std::atomic<float>* level   = nullptr;
            std::atomic<float>* tune    = nullptr;
            std::atomic<float>* octave  = nullptr;
            EnvPtrs env;
        };
        struct KSPtrs {
            std::atomic<float>* on         = nullptr;
            std::atomic<float>* level      = nullptr;
            std::atomic<float>* tune       = nullptr;
            std::atomic<float>* octave     = nullptr;
            std::atomic<float>* decay      = nullptr;
            std::atomic<float>* brightness = nullptr;
            std::atomic<float>* stiffness  = nullptr;
            std::atomic<float>* morph      = nullptr;
            std::atomic<float>* position   = nullptr;
            EnvPtrs env;
        };
        struct GranPtrs {
            std::atomic<float>* on          = nullptr;
            std::atomic<float>* triggerMode = nullptr;
            std::atomic<float>* window      = nullptr;
            std::atomic<float>* position    = nullptr;
            std::atomic<float>* size        = nullptr;
            std::atomic<float>* density     = nullptr;
            std::atomic<float>* pitch       = nullptr;
            std::atomic<float>* spray       = nullptr;
            std::atomic<float>* jitter      = nullptr;
            std::atomic<float>* pan         = nullptr;
            std::atomic<float>* reverse     = nullptr;
            std::atomic<float>* scan        = nullptr;
            std::atomic<float>* freeze      = nullptr;
            std::atomic<float>* bodyType    = nullptr;
            std::atomic<float>* bodyRoot    = nullptr;
            std::atomic<float>* bodySize    = nullptr;
            std::atomic<float>* bodyDecay   = nullptr;
            std::atomic<float>* bodyDamp    = nullptr;
            std::atomic<float>* bodyBright  = nullptr;
            std::atomic<float>* bodyInharm  = nullptr;
            std::atomic<float>* bodyExcite  = nullptr;
            std::atomic<float>* bodyMix     = nullptr;
            std::atomic<float>* bodySpread  = nullptr;
            std::atomic<float>* bodyModes   = nullptr;
            std::atomic<float>* level       = nullptr;
            std::atomic<float>* tune        = nullptr;
            std::atomic<float>* octave      = nullptr;
            EnvPtrs env;
        };
        struct TerrainPtrs {
            std::atomic<float>* on          = nullptr;
            std::atomic<float>* path        = nullptr;
            std::atomic<float>* quality     = nullptr;
            std::atomic<float>* centerX     = nullptr;
            std::atomic<float>* centerY     = nullptr;
            std::atomic<float>* radiusX     = nullptr;
            std::atomic<float>* radiusY     = nullptr;
            std::atomic<float>* zoom        = nullptr;
            std::atomic<float>* rotation    = nullptr;
            std::atomic<float>* pathSize    = nullptr;
            std::atomic<float>* ratioX      = nullptr;
            std::atomic<float>* ratioY      = nullptr;
            std::atomic<float>* scanRate    = nullptr;
            std::atomic<float>* phase       = nullptr;
            std::atomic<float>* spiral      = nullptr;
            std::atomic<float>* drive       = nullptr;
            std::atomic<float>* offset      = nullptr;
            std::atomic<float>* smoothing   = nullptr;
            std::atomic<float>* morph       = nullptr;
            std::atomic<float>* gain        = nullptr;
            std::atomic<float>* level       = nullptr;
            std::atomic<float>* tune        = nullptr;
            std::atomic<float>* octave      = nullptr;
            EnvPtrs env;
        };
        struct SlotCfgPtrs {
            std::atomic<float>* blockAMode = nullptr;
            std::atomic<float>* blockBMode = nullptr;
            std::atomic<float>* slotTypes[2][4] {};   // [block][slot]
            std::atomic<float>* osc3dDetune = nullptr;
            std::atomic<float>* fmDetune    = nullptr;
        };
        struct FilterPtrs {
            std::atomic<float>* type      = nullptr;
            std::atomic<float>* cutoff    = nullptr;
            std::atomic<float>* resonance = nullptr;
            std::atomic<float>* drive     = nullptr;
            std::atomic<float>* morph     = nullptr;
            std::atomic<float>* saturation = nullptr;
            std::atomic<float>* bassComp  = nullptr;
            std::atomic<float>* gain      = nullptr;
            std::atomic<float>* mix       = nullptr;
            std::atomic<float>* oversample = nullptr;
            std::atomic<float>* envAmt    = nullptr;
            std::atomic<float>* lfoAmt    = nullptr;
            EnvPtrs env;
        };
        struct EngineFilterPtrs {
            std::atomic<float>* on        = nullptr;
            std::atomic<float>* type      = nullptr;
            std::atomic<float>* cutoff    = nullptr;
            std::atomic<float>* resonance = nullptr;
            std::atomic<float>* drive     = nullptr;
            std::atomic<float>* morph     = nullptr;
            std::atomic<float>* saturation = nullptr;
            std::atomic<float>* bassComp  = nullptr;
            std::atomic<float>* mix       = nullptr;
            std::atomic<float>* oversample = nullptr;
            std::atomic<float>* envAmt    = nullptr;
            std::atomic<float>* lfoAmt    = nullptr;
        };
        struct OpPtrs {
            std::atomic<float>* coarse   = nullptr;
            std::atomic<float>* fine     = nullptr;
            std::atomic<float>* detune   = nullptr;
            std::atomic<float>* level    = nullptr;
            std::atomic<float>* feedback = nullptr;
            std::atomic<float>* fixed    = nullptr;
            std::atomic<float>* wave     = nullptr;
            std::atomic<float>* skirt    = nullptr;
            std::atomic<float>* resonance = nullptr;
            std::atomic<float>* keyReset = nullptr;
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
        std::array<Osc3DPtrs, kNum3DOscs> osc3d;
        FMPtrs           fm;
        FMPtrs           fmB;
        std::array<OpPtrs, 10> opsB;
        std::array<WTPtrs, kNumWTOscs> wt;
        std::array<VAPtrs, kNumVAOscs> va;
        std::array<KSPtrs, kNumKSOscs> ks;
        std::array<GranPtrs, kNumGranOscs> grain;
        std::array<TerrainPtrs, kNumTerrainOscs> terrain;
        SlotCfgPtrs      slotCfg;
        std::array<EngineFilterPtrs, kNumEngineFilters> engineFilters;
        FilterPtrs       filterA;
        FilterPtrs       filterB;
        std::atomic<float>* filterRouting = nullptr;

        std::atomic<bool> built { false };
    };
    ParamCache pc;

    void syncParams();
    void buildModSources();

    static float midiToHz(int note, float pitchBendSemitones = 0.f);
};
