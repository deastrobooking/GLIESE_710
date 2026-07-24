#include "SynthVoice.h"
#include "DSPUtils.h"

namespace
{
float pF(std::atomic<float>* p, float def = 0.f) noexcept
{
    return p ? p->load(std::memory_order_relaxed) : def;
}

int pI(std::atomic<float>* p, int def = 0) noexcept
{
    return p ? (int)p->load(std::memory_order_relaxed) : def;
}

bool pB(std::atomic<float>* p, bool def = true) noexcept
{
    return p ? p->load(std::memory_order_relaxed) > 0.5f : def;
}

float oscPitchMultiplier(std::atomic<float>* octave, std::atomic<float>* tune) noexcept
{
    const float semitones = (float)pI(octave) * 12.f + pF(tune) / 100.f;
    return gliese::dsp::pitchRatioFromSemitones(semitones);
}

enum EngineFilterIndex
{
    EngineFilter3D = 0,
    EngineFilterFM,
    EngineFilterWT,
    EngineFilterVA,
    EngineFilterKS,
    EngineFilterGran,
    EngineFilterTerrain
};

constexpr int kEngineFilterNoneMode = (int) FilterMode::NumModes;

// VA param prefix: index 0 → "osc_va_", 1 → "osc_va2_", 4 → "osc_va5_", etc.
juce::String vaPrefix(int index)
{
    return index == 0 ? juce::String("osc_va_")
                      : juce::String("osc_va") + juce::String(index + 1) + "_";
}

juce::String indexedOscPrefix(const juce::String& base, int index)
{
    return index == 0 ? juce::String("osc_") + base + "_"
                      : juce::String("osc_") + base + juce::String(index + 1) + "_";
}

EnvIndex osc3dEnvIndex(int index)
{
    return static_cast<EnvIndex>((int)ENV_3D1 + juce::jlimit(0, SynthVoice::kNum3DOscs - 1, index));
}

EnvIndex wtEnvIndex(int index)
{
    return static_cast<EnvIndex>((int)ENV_WT1 + juce::jlimit(0, SynthVoice::kNumWTOscs - 1, index));
}

EnvIndex vaEnvIndex(int index)
{
    return static_cast<EnvIndex>((int)ENV_VA1 + juce::jlimit(0, SynthVoice::kNumVAOscs - 1, index));
}

EnvIndex ksEnvIndex(int index)
{
    return static_cast<EnvIndex>((int)ENV_KS1 + juce::jlimit(0, SynthVoice::kNumKSOscs - 1, index));
}

EnvIndex granEnvIndex(int index)
{
    return static_cast<EnvIndex>((int)ENV_GRAN1 + juce::jlimit(0, SynthVoice::kNumGranOscs - 1, index));
}

EnvIndex terrainEnvIndex(int index)
{
    return static_cast<EnvIndex>((int)ENV_TERRAIN1 + juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index));
}

ModDest vaShapeDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Shape, ModDest::VA2_Shape, ModDest::VA3_Shape, ModDest::VA4_Shape,
        ModDest::VA5_Shape, ModDest::VA6_Shape, ModDest::VA7_Shape, ModDest::VA8_Shape };
    return d[juce::jlimit(0, SynthVoice::kNumVAOscs - 1, index)];
}
ModDest vaPwDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_PW, ModDest::VA2_PW, ModDest::VA3_PW, ModDest::VA4_PW,
        ModDest::VA5_PW, ModDest::VA6_PW, ModDest::VA7_PW, ModDest::VA8_PW };
    return d[juce::jlimit(0, SynthVoice::kNumVAOscs - 1, index)];
}
ModDest vaDetuneDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Detune, ModDest::VA2_Detune, ModDest::VA3_Detune, ModDest::VA4_Detune,
        ModDest::VA5_Detune, ModDest::VA6_Detune, ModDest::VA7_Detune, ModDest::VA8_Detune };
    return d[juce::jlimit(0, SynthVoice::kNumVAOscs - 1, index)];
}
ModDest vaSpreadDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Spread, ModDest::VA2_Spread, ModDest::VA3_Spread, ModDest::VA4_Spread,
        ModDest::VA5_Spread, ModDest::VA6_Spread, ModDest::VA7_Spread, ModDest::VA8_Spread };
    return d[juce::jlimit(0, SynthVoice::kNumVAOscs - 1, index)];
}
ModDest vaLevelDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Level, ModDest::VA2_Level, ModDest::VA3_Level, ModDest::VA4_Level,
        ModDest::VA5_Level, ModDest::VA6_Level, ModDest::VA7_Level, ModDest::VA8_Level };
    return d[juce::jlimit(0, SynthVoice::kNumVAOscs - 1, index)];
}
ModDest vaRingDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Ring, ModDest::VA2_Ring, ModDest::VA3_Ring, ModDest::VA4_Ring,
        ModDest::VA5_Ring, ModDest::VA6_Ring, ModDest::VA7_Ring, ModDest::VA8_Ring };
    return d[juce::jlimit(0, SynthVoice::kNumVAOscs - 1, index)];
}
ModDest vaFoldDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Fold, ModDest::VA2_Fold, ModDest::VA3_Fold, ModDest::VA4_Fold,
        ModDest::VA5_Fold, ModDest::VA6_Fold, ModDest::VA7_Fold, ModDest::VA8_Fold };
    return d[juce::jlimit(0, SynthVoice::kNumVAOscs - 1, index)];
}

// FM A uses the original FM_Op*_Level/Ratio destinations; FM B has its own
// independent set so the two engines no longer share modulation routing.
ModDest fmLevelDest(bool engineB, int opIndex)
{
    static constexpr ModDest a[] = {
        ModDest::FM_Op0_Level, ModDest::FM_Op1_Level, ModDest::FM_Op2_Level, ModDest::FM_Op3_Level,
        ModDest::FM_Op4_Level, ModDest::FM_Op5_Level, ModDest::FM_Op6_Level, ModDest::FM_Op7_Level,
        ModDest::FM_Op8_Level, ModDest::FM_Op9_Level };
    static constexpr ModDest b[] = {
        ModDest::FM_B_Op0_Level, ModDest::FM_B_Op1_Level, ModDest::FM_B_Op2_Level, ModDest::FM_B_Op3_Level,
        ModDest::FM_B_Op4_Level, ModDest::FM_B_Op5_Level, ModDest::FM_B_Op6_Level, ModDest::FM_B_Op7_Level,
        ModDest::FM_B_Op8_Level, ModDest::FM_B_Op9_Level };
    return (engineB ? b : a)[juce::jlimit(0, 9, opIndex)];
}
ModDest fmRatioDest(bool engineB, int opIndex)
{
    static constexpr ModDest a[] = {
        ModDest::FM_Op0_Ratio, ModDest::FM_Op1_Ratio, ModDest::FM_Op2_Ratio, ModDest::FM_Op3_Ratio,
        ModDest::FM_Op4_Ratio, ModDest::FM_Op5_Ratio, ModDest::FM_Op6_Ratio, ModDest::FM_Op7_Ratio,
        ModDest::FM_Op8_Ratio, ModDest::FM_Op9_Ratio };
    static constexpr ModDest b[] = {
        ModDest::FM_B_Op0_Ratio, ModDest::FM_B_Op1_Ratio, ModDest::FM_B_Op2_Ratio, ModDest::FM_B_Op3_Ratio,
        ModDest::FM_B_Op4_Ratio, ModDest::FM_B_Op5_Ratio, ModDest::FM_B_Op6_Ratio, ModDest::FM_B_Op7_Ratio,
        ModDest::FM_B_Op8_Ratio, ModDest::FM_B_Op9_Ratio };
    return (engineB ? b : a)[juce::jlimit(0, 9, opIndex)];
}

// Per-slot terrain destinations. Slot 0 keeps the original shared Terrain_*
// names; slots 1-7 (Terrain2..Terrain8) give each terrain oscillator its own
// independent matrix offsets, matching the VA2..VA8 convention.
ModDest terrainCenterXDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::Terrain_CenterX, ModDest::Terrain2_CenterX, ModDest::Terrain3_CenterX, ModDest::Terrain4_CenterX,
        ModDest::Terrain5_CenterX, ModDest::Terrain6_CenterX, ModDest::Terrain7_CenterX, ModDest::Terrain8_CenterX };
    return d[juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index)];
}
ModDest terrainCenterYDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::Terrain_CenterY, ModDest::Terrain2_CenterY, ModDest::Terrain3_CenterY, ModDest::Terrain4_CenterY,
        ModDest::Terrain5_CenterY, ModDest::Terrain6_CenterY, ModDest::Terrain7_CenterY, ModDest::Terrain8_CenterY };
    return d[juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index)];
}
ModDest terrainRadiusXDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::Terrain_RadiusX, ModDest::Terrain2_RadiusX, ModDest::Terrain3_RadiusX, ModDest::Terrain4_RadiusX,
        ModDest::Terrain5_RadiusX, ModDest::Terrain6_RadiusX, ModDest::Terrain7_RadiusX, ModDest::Terrain8_RadiusX };
    return d[juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index)];
}
ModDest terrainRadiusYDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::Terrain_RadiusY, ModDest::Terrain2_RadiusY, ModDest::Terrain3_RadiusY, ModDest::Terrain4_RadiusY,
        ModDest::Terrain5_RadiusY, ModDest::Terrain6_RadiusY, ModDest::Terrain7_RadiusY, ModDest::Terrain8_RadiusY };
    return d[juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index)];
}
ModDest terrainRatioXDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::Terrain_RatioX, ModDest::Terrain2_RatioX, ModDest::Terrain3_RatioX, ModDest::Terrain4_RatioX,
        ModDest::Terrain5_RatioX, ModDest::Terrain6_RatioX, ModDest::Terrain7_RatioX, ModDest::Terrain8_RatioX };
    return d[juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index)];
}
ModDest terrainRatioYDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::Terrain_RatioY, ModDest::Terrain2_RatioY, ModDest::Terrain3_RatioY, ModDest::Terrain4_RatioY,
        ModDest::Terrain5_RatioY, ModDest::Terrain6_RatioY, ModDest::Terrain7_RatioY, ModDest::Terrain8_RatioY };
    return d[juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index)];
}
ModDest terrainSmoothingDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::Terrain_Smoothing, ModDest::Terrain2_Smoothing, ModDest::Terrain3_Smoothing, ModDest::Terrain4_Smoothing,
        ModDest::Terrain5_Smoothing, ModDest::Terrain6_Smoothing, ModDest::Terrain7_Smoothing, ModDest::Terrain8_Smoothing };
    return d[juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index)];
}
ModDest terrainMorphDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::Terrain_Morph, ModDest::Terrain2_Morph, ModDest::Terrain3_Morph, ModDest::Terrain4_Morph,
        ModDest::Terrain5_Morph, ModDest::Terrain6_Morph, ModDest::Terrain7_Morph, ModDest::Terrain8_Morph };
    return d[juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index)];
}
ModDest terrainDriveDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::Terrain_Drive, ModDest::Terrain2_Drive, ModDest::Terrain3_Drive, ModDest::Terrain4_Drive,
        ModDest::Terrain5_Drive, ModDest::Terrain6_Drive, ModDest::Terrain7_Drive, ModDest::Terrain8_Drive };
    return d[juce::jlimit(0, SynthVoice::kNumTerrainOscs - 1, index)];
}

ModDest engineFilterCutoffDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::EngFilt3D_Cutoff, ModDest::EngFiltFM_Cutoff, ModDest::EngFiltWT_Cutoff,
        ModDest::EngFiltVA_Cutoff, ModDest::EngFiltKS_Cutoff, ModDest::EngFiltGran_Cutoff,
        ModDest::EngFiltTerrain_Cutoff };
    return d[juce::jlimit(0, SynthVoice::kNumEngineFilters - 1, index)];
}

ModDest engineFilterResDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::EngFilt3D_Resonance, ModDest::EngFiltFM_Resonance, ModDest::EngFiltWT_Resonance,
        ModDest::EngFiltVA_Resonance, ModDest::EngFiltKS_Resonance, ModDest::EngFiltGran_Resonance,
        ModDest::EngFiltTerrain_Resonance };
    return d[juce::jlimit(0, SynthVoice::kNumEngineFilters - 1, index)];
}

ModDest engineFilterDriveDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::EngFilt3D_Drive, ModDest::EngFiltFM_Drive, ModDest::EngFiltWT_Drive,
        ModDest::EngFiltVA_Drive, ModDest::EngFiltKS_Drive, ModDest::EngFiltGran_Drive,
        ModDest::EngFiltTerrain_Drive };
    return d[juce::jlimit(0, SynthVoice::kNumEngineFilters - 1, index)];
}

ModDest engineFilterMorphDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::EngFilt3D_Morph, ModDest::EngFiltFM_Morph, ModDest::EngFiltWT_Morph,
        ModDest::EngFiltVA_Morph, ModDest::EngFiltKS_Morph, ModDest::EngFiltGran_Morph,
        ModDest::EngFiltTerrain_Morph };
    return d[juce::jlimit(0, SynthVoice::kNumEngineFilters - 1, index)];
}

ModDest engineFilterMixDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::EngFilt3D_Mix, ModDest::EngFiltFM_Mix, ModDest::EngFiltWT_Mix,
        ModDest::EngFiltVA_Mix, ModDest::EngFiltKS_Mix, ModDest::EngFiltGran_Mix,
        ModDest::EngFiltTerrain_Mix };
    return d[juce::jlimit(0, SynthVoice::kNumEngineFilters - 1, index)];
}
} // namespace

float SynthVoice::midiToHz(int note, float pitchBendSemitones)
{
    return 440.f * gliese::dsp::pitchRatioFromSemitones((float)(note - 69) + pitchBendSemitones);
}

void SynthVoice::prepare(double sr)
{
    sampleRate = sr;
    for (auto& osc : osc3dArray) osc.prepare(sr);
    fm.prepare(sr);
    fm2.prepare(sr);
    for (auto& wt : wtOscs) wt.prepare(sr);
    for (auto& va : vaOscs) va.prepare(sr);
    for (auto& ks : ksOscs) ks.prepare(sr);
    for (auto& grain : granularOscs) grain.prepare(sr);
    for (auto& terrain : terrainOscs) terrain.prepare(sr);
    envBank.prepare(sr);
    lfo.prepare(sr);
    for (auto& filter : engineFilterL) filter.prepare(sr);
    for (auto& filter : engineFilterR) filter.prepare(sr);
    dualFilter.prepare(sr);

    auto prepareSmooth = [sr](auto& smooth) {
        smooth.reset(sr, 0.01);
        smooth.setCurrentAndTargetValue(0.f);
    };
    prepareSmooth(masterVolumeSmooth);
    masterPanSmooth.reset(sr, 0.01);
    masterPanSmooth.setCurrentAndTargetValue(0.f);
    for (auto& smooth : fmLevelSmooth) prepareSmooth(smooth);
    for (auto& smooth : osc3dLevelSmooth) prepareSmooth(smooth);
    for (auto& smooth : wtLevelSmooth) prepareSmooth(smooth);
    for (auto& smooth : ksLevelSmooth) prepareSmooth(smooth);
    for (auto& smooth : grainLevelSmooth) prepareSmooth(smooth);
    for (auto& smooth : terrainLevelSmooth) prepareSmooth(smooth);
    for (auto& smooth : vaLevelSmooth) prepareSmooth(smooth);
}

void SynthVoice::reset()
{
    midiNote = -1;
    midiChannel = 1;
    age      = 0;
    for (auto& osc : osc3dArray) osc.reset();
    fm.reset();
    fm2.reset();
    for (auto& wt : wtOscs) wt.reset();
    for (auto& va : vaOscs) va.reset();
    for (auto& ks : ksOscs) ks.reset();
    for (auto& grain : granularOscs) grain.reset();
    for (auto& terrain : terrainOscs) terrain.reset();
    envBank.reset();
    lfo.reset();
    for (auto& filter : engineFilterL) filter.reset();
    for (auto& filter : engineFilterR) filter.reset();
    dualFilter.reset();
    smoothingInitialized = false;
}

void SynthVoice::noteOn(int note, float vel, int channel)
{
    if (!isActive())
        smoothingInitialized = false;
    midiNote = note;
    midiChannel = juce::jlimit(1, 16, channel);
    velocity = juce::jlimit(0.f, 1.f, vel);
    age      = 0;

    portaTargetHz = midiToHz(note);
    if (portaHz < 1.f)
        portaHz = portaTargetHz;

    float hz = portaHz;
    for (auto& osc : osc3dArray) osc.setFrequency(hz);
    fm.setNoteFrequency(hz);
    fm2.setNoteFrequency(hz);
    for (auto& wt : wtOscs) wt.setFrequency(hz);
    if (pc.built)
    {
        for (int i = 0; i < kNumWTOscs; ++i)
        {
            const auto& cwt = pc.wt[(size_t)i];
            WavetableOsc::Params wtp;
            wtp.warpMode = static_cast<WavetableOsc::WarpMode>(pI(cwt.warpMode));
            wtp.warpAmount = pF(cwt.warp);
            wtp.pdBreakpoint = pF(cwt.pdBreak, 0.75f);
            wtp.unison = pI(cwt.unison, 1);
            wtp.detune = pF(cwt.detune);
            wtp.spread = pF(cwt.spread);
            wtp.positionSpread = pF(cwt.posSpread);
            wtp.phaseOffset = pF(cwt.phase);
            wtp.randomPhase = pF(cwt.random);
            wtp.retrigger = pB(cwt.retrig, true);
            wtOscs[(size_t)i].setParams(wtp);
        }
    }
    for (auto& wt : wtOscs) wt.noteOn();
    for (auto& va : vaOscs) va.setFrequency(hz);
    for (int i = 0; i < kNumKSOscs; ++i)
    {
        auto& ks = ksOscs[(size_t)i];
        ks.setFrequency(hz);
        ks.pluck(velocity, cachedKSParams[(size_t)i]);
    }
    for (auto& grain : granularOscs)
    {
        grain.setFrequency(hz);
        grain.noteOn(note, velocity);
    }
    for (auto& terrain : terrainOscs)
        terrain.setFrequency(hz);

    envBank.noteOn();
    fm.noteOn();
    fm2.noteOn();
    lfo.noteOn();
    dualFilter.noteOn();
}

void SynthVoice::noteOff()
{
    envBank.noteOff();
    fm.noteOff();
    fm2.noteOff();
    for (auto& grain : granularOscs) grain.noteOff();
    dualFilter.noteOff();
}

void SynthVoice::setHostBPM(double bpm)  { lfo.setHostBPM(bpm); }

bool SynthVoice::setTerrainHeightmap(int index, const juce::Image& image)
{
    if (index < 0 || index >= kNumTerrainOscs)
        return false;

    return terrainOscs[(size_t)index].setHeightmapFromImage(image);
}

void SynthVoice::shareTerrainHeightmap(int index, const SynthVoice& source)
{
    if (index < 0 || index >= kNumTerrainOscs)
        return;
    terrainOscs[(size_t) index].shareTerrainFrom(source.terrainOscs[(size_t) index]);
}

void SynthVoice::setMidiCC(ModSource src, float value)
{
    int idx = (int)src;
    if (idx >= 0 && idx < (int)ModSource::NumSources)
        midiCCs[idx] = value;
}

bool SynthVoice::isActive() const
{
    return midiNote >= 0 && envBank.anyActive();
}

void SynthVoice::addWavetable(juce::String name, const float* samples, int numSamples)
{
    for (auto& wt : wtOscs)
        wt.addTable(name, samples, numSamples);
}

//──────────────────────────────────────────────────────────────────────────────
void SynthVoice::buildParamCache(juce::AudioProcessorValueTreeState& apvts)
{
    auto get = [&](const juce::String& id) { return apvts.getRawParameterValue(id); };
    auto cacheEnv = [&](ParamCache::EnvPtrs& env, const juce::String& pfx) {
        env.a = get(pfx + "adsr_a");
        env.d = get(pfx + "adsr_d");
        env.s = get(pfx + "adsr_s");
        env.r = get(pfx + "adsr_r");
    };

    pc.master.volume     = get("master_volume");
    pc.master.tune       = get("master_tune");
    pc.master.octave     = get("master_octave");
    pc.master.pan        = get("master_pan");
    pc.master.portamento = get("master_portamento");
    pc.master.pbRange    = get("master_pb_range");
    pc.master.mwRange    = get("master_mw_range");
    cacheEnv(pc.master.env, "master_");

    for (int i = 0; i < kNum3DOscs; ++i)
    {
        auto pfx = indexedOscPrefix("3d", i);
        auto& c = pc.osc3d[(size_t)i];
        c.field     = get(pfx + "field");
        c.path      = get(pfx + "path");
        c.radius    = get(pfx + "radius");
        c.depth     = get(pfx + "depth");
        c.warp      = get(pfx + "warp");
        c.rotation  = get(pfx + "rotation");
        c.chaos     = get(pfx + "chaos");
        c.morph     = get(pfx + "morph");
        c.fieldFreq = get(pfx + "field_freq");
        c.zRatio    = get(pfx + "z_ratio");
        c.fold      = get(pfx + "fold");
        c.drive     = get(pfx + "drive");
        c.on        = get(pfx + "on");
        c.level     = get(pfx + "level");
        c.tune      = get(pfx + "tune");
        c.octave    = get(pfx + "octave");
        cacheEnv(c.env, pfx);
    }

    auto cacheFM = [&](ParamCache::FMPtrs& f, const juce::String& pfx)
    {
        f.on         = get(pfx + "on");
        f.algorithm  = get(pfx + "algorithm");
        f.level      = get(pfx + "level");
        f.tune       = get(pfx + "tune");
        f.octave     = get(pfx + "octave");
        f.detune     = get(pfx + "detune");
        f.phase      = get(pfx + "phase");
        f.algoMorph  = get(pfx + "algo_morph");
        f.indexScale = get(pfx + "index");
        cacheEnv(f.env, pfx);
    };
    cacheFM(pc.fm,  "osc_fm_");
    cacheFM(pc.fmB, "osc_fm2_");

    for (int i = 0; i < kNumWTOscs; ++i)
    {
        auto pfx = indexedOscPrefix("wt", i);
        auto& c = pc.wt[(size_t)i];
        c.on     = get(pfx + "on");
        c.table  = get(pfx + "table");
        c.pos    = get(pfx + "pos");
        c.warpMode = get(pfx + "warp_mode");
        c.warp   = get(pfx + "warp");
        c.pdBreak = get(pfx + "pd_break");
        c.unison = get(pfx + "unison");
        c.detune = get(pfx + "detune");
        c.spread = get(pfx + "spread");
        c.posSpread = get(pfx + "pos_spread");
        c.phase  = get(pfx + "phase");
        c.random = get(pfx + "random");
        c.retrig = get(pfx + "retrig");
        c.level  = get(pfx + "level");
        c.tune   = get(pfx + "tune");
        c.octave = get(pfx + "octave");
        cacheEnv(c.env, pfx);
    }

    for (int i = 0; i < kNumVAOscs; ++i)
    {
        juce::String pfx = vaPrefix(i);
        auto& v = pc.va[(size_t)i];
        v.on     = get(pfx + "on");
        v.model  = get(pfx + "model");
        v.shape  = get(pfx + "shape");
        v.pw     = get(pfx + "pw");
        v.detune = get(pfx + "detune");
        v.unison = get(pfx + "unison");
        v.spread = get(pfx + "spread");
        v.ring   = get(pfx + "ring");
        v.fold   = get(pfx + "fold");
        v.level  = get(pfx + "level");
        v.tune   = get(pfx + "tune");
        v.octave = get(pfx + "octave");
        cacheEnv(v.env, pfx);
    }

    for (int i = 0; i < kNumKSOscs; ++i)
    {
        auto pfx = indexedOscPrefix("ks", i);
        auto& c = pc.ks[(size_t)i];
        c.on         = get(pfx + "on");
        c.level      = get(pfx + "level");
        c.tune       = get(pfx + "tune");
        c.octave     = get(pfx + "octave");
        c.decay      = get(pfx + "decay");
        c.brightness = get(pfx + "brightness");
        c.stiffness  = get(pfx + "stiffness");
        c.morph      = get(pfx + "morph");
        c.position   = get(pfx + "position");
        cacheEnv(c.env, pfx);
    }

    for (int i = 0; i < kNumGranOscs; ++i)
    {
        auto pfx = indexedOscPrefix("grain", i);
        auto& c = pc.grain[(size_t)i];
        c.on          = get(pfx + "on");
        c.triggerMode = get(pfx + "trigger_mode");
        c.window      = get(pfx + "window");
        c.position    = get(pfx + "position");
        c.size        = get(pfx + "size");
        c.density     = get(pfx + "density");
        c.pitch       = get(pfx + "pitch");
        c.spray       = get(pfx + "spray");
        c.jitter      = get(pfx + "jitter");
        c.pan         = get(pfx + "pan");
        c.reverse     = get(pfx + "reverse");
        c.scan        = get(pfx + "scan");
        c.freeze      = get(pfx + "freeze");
        c.bodyType    = get(pfx + "body_type");
        c.bodyRoot    = get(pfx + "body_root");
        c.bodySize    = get(pfx + "body_size");
        c.bodyDecay   = get(pfx + "body_decay");
        c.bodyDamp    = get(pfx + "body_damp");
        c.bodyBright  = get(pfx + "body_bright");
        c.bodyInharm  = get(pfx + "body_inharm");
        c.bodyExcite  = get(pfx + "body_excite");
        c.bodyMix     = get(pfx + "body_mix");
        c.bodySpread  = get(pfx + "body_spread");
        c.bodyModes   = get(pfx + "body_modes");
        c.level       = get(pfx + "level");
        c.tune        = get(pfx + "tune");
        c.octave      = get(pfx + "octave");
        cacheEnv(c.env, pfx);
    }

    for (int i = 0; i < kNumTerrainOscs; ++i)
    {
        auto pfx = indexedOscPrefix("terrain", i);
        auto& c = pc.terrain[(size_t)i];
        c.on        = get(pfx + "on");
        c.path      = get(pfx + "path");
        c.quality   = get(pfx + "quality");
        c.centerX   = get(pfx + "center_x");
        c.centerY   = get(pfx + "center_y");
        c.radiusX   = get(pfx + "radius_x");
        c.radiusY   = get(pfx + "radius_y");
        c.zoom      = get(pfx + "zoom");
        c.rotation  = get(pfx + "rotation");
        c.pathSize  = get(pfx + "path_size");
        c.ratioX    = get(pfx + "ratio_x");
        c.ratioY    = get(pfx + "ratio_y");
        c.scanRate  = get(pfx + "scan_rate");
        c.phase     = get(pfx + "phase");
        c.spiral    = get(pfx + "spiral");
        c.drive     = get(pfx + "drive");
        c.offset    = get(pfx + "offset");
        c.smoothing = get(pfx + "smoothing");
        c.morph     = get(pfx + "morph");
        c.gain      = get(pfx + "gain");
        c.level     = get(pfx + "level");
        c.tune      = get(pfx + "tune");
        c.octave    = get(pfx + "octave");
        cacheEnv(c.env, pfx);
    }

    // Slot configuration
    pc.slotCfg.blockAMode   = get("engine_block_a_mode");
    pc.slotCfg.blockBMode   = get("engine_block_b_mode");
    pc.slotCfg.osc3dDetune  = get("engine_3d_detune");
    pc.slotCfg.fmDetune     = get("engine_fm_detune");
    for (int b = 0; b < 2; ++b)
    {
        for (int s = 0; s < 4; ++s)
        {
            juce::String id = juce::String("engine_block_") + (b == 0 ? "a" : "b")
                              + "_slot_" + juce::String(s);
            pc.slotCfg.slotTypes[b][s] = get(id);
        }
    }

    static const juce::String engineFilterPrefixes[] {
        "engine_filter_3d_", "engine_filter_fm_", "engine_filter_wt_",
        "engine_filter_va_", "engine_filter_ks_", "engine_filter_gran_",
        "engine_filter_terrain_"
    };
    for (int i = 0; i < kNumEngineFilters; ++i)
    {
        auto& f = pc.engineFilters[(size_t) i];
        const auto& pfx = engineFilterPrefixes[(size_t) i];
        f.on         = get(pfx + "on");
        f.type       = get(pfx + "type");
        f.cutoff     = get(pfx + "cutoff");
        f.resonance  = get(pfx + "resonance");
        f.drive      = get(pfx + "drive");
        f.morph      = get(pfx + "morph");
        f.saturation = get(pfx + "saturation");
        f.bassComp   = get(pfx + "bass_comp");
        f.mix        = get(pfx + "mix");
        f.oversample = get(pfx + "oversample");
        f.envAmt     = get(pfx + "env_amt");
        f.lfoAmt     = get(pfx + "lfo_amt");
    }

    auto cacheFilter = [&](ParamCache::FilterPtrs& f, const juce::String& pfx) {
        f.type      = get(pfx + "type");
        f.cutoff    = get(pfx + "cutoff");
        f.resonance = get(pfx + "resonance");
        f.drive     = get(pfx + "drive");
        f.morph     = get(pfx + "morph");
        f.saturation = get(pfx + "saturation");
        f.bassComp  = get(pfx + "bass_comp");
        f.gain      = get(pfx + "gain");
        f.mix       = get(pfx + "mix");
        f.oversample = get(pfx + "oversample");
        f.envAmt    = get(pfx + "env_amt");
        f.lfoAmt    = get(pfx + "lfo_amt");
        cacheEnv(f.env, pfx);
    };
    cacheFilter(pc.filterA, "filter_a_");
    cacheFilter(pc.filterB, "filter_b_");
    pc.filterRouting = get("filter_routing");

    auto cacheOps = [&](std::array<ParamCache::OpPtrs, 10>& ops, const juce::String& base)
    {
        for (int i = 0; i < 10; ++i)
        {
            juce::String pfx = base + juce::String(i) + "_";
            auto& o  = ops[(size_t)i];
            o.coarse   = get(pfx + "coarse");
            o.fine     = get(pfx + "fine");
            o.detune   = get(pfx + "detune");
            o.level    = get(pfx + "level");
            o.feedback = get(pfx + "feedback");
            o.fixed    = get(pfx + "fixed");
            o.wave     = get(pfx + "wave");
            o.skirt    = get(pfx + "skirt");
            o.resonance = get(pfx + "resonance");
            o.keyReset = get(pfx + "key_reset");
            o.vel      = get(pfx + "vel");
            o.key      = get(pfx + "key");
            o.adsrA    = get(pfx + "adsr_a");
            o.adsrD    = get(pfx + "adsr_d");
            o.adsrS    = get(pfx + "adsr_s");
            o.adsrR    = get(pfx + "adsr_r");
        }
    };
    cacheOps(pc.ops,  "op");
    cacheOps(pc.opsB, "op2_");

    for (int i = 0; i < 4; ++i)
    {
        juce::String pfx = "lfo" + juce::String(i + 1) + "_";
        auto& l  = pc.lfos[(size_t)i];
        l.shape    = get(pfx + "shape");
        l.rate     = get(pfx + "rate");
        l.sync     = get(pfx + "sync");
        l.syncRate = get(pfx + "sync_rate");
        l.amount   = get(pfx + "amount");
        l.phase    = get(pfx + "phase");
        l.retrig   = get(pfx + "retrig");
    }

    for (int i = 0; i < 32; ++i)
    {
        juce::String pfx = "mod" + juce::String(i) + "_";
        auto& m  = pc.mods[(size_t)i];
        m.source  = get(pfx + "source");
        m.dest    = get(pfx + "dest");
        m.amount  = get(pfx + "amount");
        m.bipolar = get(pfx + "bipolar");
    }

    for (int i = 0; i < 8; ++i)
        pc.macros[(size_t)i] = get("macro" + juce::String(i + 1));

    pc.built.store(true, std::memory_order_release);
}

//──────────────────────────────────────────────────────────────────────────────
void SynthVoice::syncParams()
{
    auto envParams = [](const ParamCache::EnvPtrs& e, float a, float d, float s, float r) {
        ADSREnvelope::Params ep;
        ep.attackSec  = pF(e.a, a);
        ep.decaySec   = pF(e.d, d);
        ep.sustain    = pF(e.s, s);
        ep.releaseSec = pF(e.r, r);
        return ep;
    };

    // ── Resolve block/slot configuration ─────────────────────────────────────
    const bool bothPagesAreFM = pI(pc.slotCfg.blockAMode, 0) == 1
                             && pI(pc.slotCfg.blockBMode, 0) == 1;
    currentBlockAIsFM = bothPagesAreFM;
    currentBlockBIsFM = bothPagesAreFM;

    current3dCount = 0;
    currentVACount = 0;
    currentWTCount = 0;
    currentKSCount = 0;
    currentGranCount = 0;
    currentTerrainCount = 0;

    for (int b = 0; b < 2; ++b)
    {
        bool isFM = (b == 0) ? currentBlockAIsFM : currentBlockBIsFM;
        if (isFM) continue;
        for (int s = 0; s < 4; ++s)
        {
            int t = pI(pc.slotCfg.slotTypes[b][s], 0);
            const int slotIndex = b * 4 + s;
            if      (t == 1 && current3dCount < kNum3DOscs) active3dIndices[(size_t)current3dCount++] = slotIndex;
            else if (t == 2 && currentVACount < kNumVAOscs) activeVAIndices[(size_t)currentVACount++] = slotIndex;
            else if (t == 3 && currentWTCount < kNumWTOscs) activeWTIndices[(size_t)currentWTCount++] = slotIndex;
            else if (t == 4 && currentKSCount < kNumKSOscs) activeKSIndices[(size_t)currentKSCount++] = slotIndex;
            else if (t == 5 && currentGranCount < kNumGranOscs) activeGranIndices[(size_t)currentGranCount++] = slotIndex;
            else if (t == 6 && currentTerrainCount < kNumTerrainOscs)
            {
                activeTerrainIndices[(size_t)currentTerrainCount++] = slotIndex;
                if (s < 3)
                    ++s;
            }
        }
    }

    // ── Portamento glide ──────────────────────────────────────────────────────
    float portaTime = pF(pc.master.portamento, 0.f);
    if (portaTime > 0.f)
    {
        float coef = std::exp(-1.f / (portaTime * (float)sampleRate));
        portaHz = portaTargetHz + coef * (portaHz - portaTargetHz);
    }
    else portaHz = portaTargetHz;

    // ── Global pitch ──────────────────────────────────────────────────────────
    float pitchMod   = modDests.dests[(int)ModDest::OscPitch_All];
    float tuneSemi   = pF(pc.master.tune) / 100.f;
    float octaveSemi = (float)pI(pc.master.octave) * 12.f;
    float pbEffect   = midiCCs[(int)ModSource::PitchBend] * pF(pc.master.pbRange, 2.f);
    float totalPitch = pitchMod * 12.f + tuneSemi + octaveSemi + pbEffect;
    float pitchedHz  = portaHz * gliese::dsp::pitchRatioFromSemitones(totalPitch);

    auto engineFilterSourceActive = [&](int filterIndex) {
        switch (filterIndex)
        {
            case EngineFilter3D:      return current3dCount > 0;
            case EngineFilterFM:      return currentBlockAIsFM || currentBlockBIsFM;
            case EngineFilterWT:      return currentWTCount > 0;
            case EngineFilterVA:      return currentVACount > 0;
            case EngineFilterKS:      return currentKSCount > 0;
            case EngineFilterGran:    return currentGranCount > 0;
            case EngineFilterTerrain: return currentTerrainCount > 0;
            default:                  return false;
        }
    };

    // 3D instances — symmetric detune per active 3D slot, independent controls
    float detuneSemis = pF(pc.slotCfg.osc3dDetune, 0.f);
    for (int n = 0; n < current3dCount; ++n)
    {
        const int i = active3dIndices[(size_t)n];
        const auto& c3d = pc.osc3d[(size_t)i];
        Osc3D::Params p3d;
        p3d.field    = static_cast<Osc3D::FieldType>(pI(c3d.field));
        p3d.path     = static_cast<Osc3D::PathType>(pI(c3d.path));
        p3d.radius   = juce::jlimit(0.1f, 4.0f, pF(c3d.radius, 1.f) + modDests.dests[(int)ModDest::Osc3D_Radius]);
        p3d.depth    = juce::jlimit(0.0f, 4.0f, pF(c3d.depth, 1.f)  + modDests.dests[(int)ModDest::Osc3D_Depth]);
        p3d.warpAmt  = juce::jlimit(0.0f, 2.0f, pF(c3d.warp, 0.25f) + modDests.dests[(int)ModDest::Osc3D_Warp]);
        p3d.rotation = pF(c3d.rotation);
        p3d.chaos    = juce::jlimit(0.f, 1.f, pF(c3d.chaos)       + modDests.dests[(int)ModDest::Osc3D_Chaos]);
        p3d.morphT   = juce::jlimit(0.f, 1.f, pF(c3d.morph)       + modDests.dests[(int)ModDest::Osc3D_Morph]);
        p3d.fieldFreq = juce::jlimit(0.25f, 16.0f, pF(c3d.fieldFreq, 1.f) + modDests.dests[(int)ModDest::Osc3D_FieldFreq]);
        p3d.zRatio    = juce::jlimit(0.25f, 4.0f,  pF(c3d.zRatio, 1.f)    + modDests.dests[(int)ModDest::Osc3D_ZRatio]);
        p3d.foldAmt   = juce::jlimit(0.f, 1.f,     pF(c3d.fold)            + modDests.dests[(int)ModDest::Osc3D_Fold]);
        p3d.drive     = juce::jlimit(0.25f, 4.0f,  pF(c3d.drive, 1.f)      + modDests.dests[(int)ModDest::Osc3D_Drive]);

        osc3dArray[(size_t)i].setParams(p3d);
        float offset = 0.f;
        if (current3dCount > 1 && detuneSemis > 0.f)
        {
            float pos = (float)n / (float)(current3dCount - 1) * 2.f - 1.f;
            offset = pos * detuneSemis * 0.5f;
        }
        const float baseHz3d = pitchedHz * oscPitchMultiplier(c3d.octave, c3d.tune);
        osc3dArray[(size_t)i].setFrequency(baseHz3d * gliese::dsp::pitchRatioFromSemitones(offset));
        envBank.get(osc3dEnvIndex(i)).setParams(envParams(c3d.env, 0.01f, 0.2f, 1.f, 0.3f));
    }

    // ── FM Engines ────────────────────────────────────────────────────────────
    auto configureFM = [&](FMAlgorithm& engine, const ParamCache::FMPtrs& cfm,
                           const std::array<ParamCache::OpPtrs, 10>& ops,
                           EnvIndex envIndex, float extraDetuneCents, bool engineB)
    {
        const float detuneCents = pF(cfm.detune) + extraDetuneCents;
        const float fmHz = pitchedHz * oscPitchMultiplier(cfm.octave, cfm.tune)
                                  * gliese::dsp::pitchRatioFromCents(detuneCents);
        engine.setAlgorithm(pI(cfm.algorithm));
        engine.setAlgorithmMorph(pF(cfm.algoMorph));
        engine.setNoteFrequency(fmHz);
        engine.setVelocity(velocity);
        engine.setMidiNote(midiNote);

        ADSREnvelope::Params fmLayerEnv = envParams(cfm.env, 0.01f, 0.2f, 1.f, 0.3f);
        envBank.get(envIndex).setParams(fmLayerEnv);

        const float phaseOffset = pF(cfm.phase);
        const float indexScale = juce::jlimit(0.f, 2.f, pF(cfm.indexScale, 1.f));

        for (int i = 0; i < 10; ++i)
        {
            const auto& opc = ops[(size_t)i];
            FMOperator::Params op;
            float ratioMod = modDests.dests[(int)fmRatioDest(engineB, i)];
            float levelMod = modDests.dests[(int)fmLevelDest(engineB, i)];
            op.coarse        = pI(opc.coarse, 1);
            op.fine          = pF(opc.fine);
            op.detune        = pF(opc.detune);
            op.ratioOffset   = ratioMod;
            op.level         = juce::jlimit(0.f, 1.f, (pF(opc.level, 1.f) + levelMod) * indexScale);
            op.feedback      = pF(opc.feedback);
            op.fixedFreq     = pI(opc.fixed) != 0;
            op.waveShape     = static_cast<FMOperator::Wave>(pI(opc.wave));
            op.spectralSkirt = pF(opc.skirt, 0.35f);
            op.spectralResonance = pF(opc.resonance, 0.25f);
            op.velocityDepth = pF(opc.vel);
            op.keyScaleDepth = pF(opc.key);
            op.phaseOffset   = phaseOffset;
            op.keyOnReset    = pB(opc.keyReset, true);
            engine.getOperator(i).setParams(op);

            ADSREnvelope::Params ep;
            ep.attackSec  = pF(opc.adsrA, 0.01f);
            ep.decaySec   = pF(opc.adsrD, 0.5f);
            ep.sustain    = pF(opc.adsrS, 0.8f);
            ep.releaseSec = juce::jmax(pF(opc.adsrR, 0.3f), fmLayerEnv.releaseSec);
            engine.getOperator(i).getEnvelope().setParams(ep);
        }
    };

    if (currentBlockAIsFM)
        configureFM(fm, pc.fm, pc.ops, ENV_FM, 0.f, false);
    if (currentBlockBIsFM)
        configureFM(fm2, pc.fmB, pc.opsB, ENV_FM2,
                    pF(pc.slotCfg.fmDetune, 0.f), true);

    // ── Wavetable ─────────────────────────────────────────────────────────────
    for (int n = 0; n < currentWTCount; ++n)
    {
        const int i = activeWTIndices[(size_t)n];
        const auto& cwt = pc.wt[(size_t)i];
        float wtHz = pitchedHz * oscPitchMultiplier(cwt.octave, cwt.tune);
        WavetableOsc::Params wtParams;
        wtParams.warpMode = static_cast<WavetableOsc::WarpMode>(pI(cwt.warpMode));
        wtParams.warpAmount = juce::jlimit(0.f, 1.f,
            pF(cwt.warp) + modDests.dests[(int)ModDest::WT_Warp]);
        wtParams.pdBreakpoint = juce::jlimit(0.f, 1.f,
            pF(cwt.pdBreak, 0.75f) + modDests.dests[(int)ModDest::WT_PD_Break]);
        wtParams.unison = pI(cwt.unison, 1);
        wtParams.detune = pF(cwt.detune);
        wtParams.spread = pF(cwt.spread);
        wtParams.positionSpread = pF(cwt.posSpread);
        wtParams.phaseOffset = pF(cwt.phase);
        wtParams.randomPhase = pF(cwt.random);
        wtParams.retrigger = pB(cwt.retrig, true);
        auto& wt = wtOscs[(size_t)i];
        wt.setFrequency(wtHz);
        wt.setParams(wtParams);
        wt.setTablePosition(pI(cwt.table), pF(cwt.pos) + modDests.dests[(int)ModDest::WT_Position]);
        envBank.get(wtEnvIndex(i)).setParams(envParams(cwt.env, 0.01f, 0.2f, 1.f, 0.3f));
    }

    // ── VA Oscillators ────────────────────────────────────────────────────────
    for (int n = 0; n < currentVACount; ++n)
    {
        const int i = activeVAIndices[(size_t)n];
        const auto& cv = pc.va[(size_t)i];
        float vaHz = pitchedHz * oscPitchMultiplier(cv.octave, cv.tune);
        vaOscs[(size_t)i].setFrequency(vaHz);

        VAOscillator::Params vap;
        vap.model       = static_cast<VAOscillator::Model>(pI(cv.model));
        vap.shapeMorph  = juce::jlimit(0.f, 5.f,
                            pF(cv.shape, 1.f) + modDests.dests[(int)vaShapeDest(i)] * 5.f);
        vap.pulseWidth  = juce::jlimit(0.05f, 0.95f,
                            pF(cv.pw, 0.5f) + modDests.dests[(int)vaPwDest(i)]);
        vap.detune      = pF(cv.detune) + modDests.dests[(int)vaDetuneDest(i)];
        vap.unisonCount = pI(cv.unison, 1);
        vap.spread      = pF(cv.spread, 0.5f) + modDests.dests[(int)vaSpreadDest(i)];
        vap.ringMod     = juce::jlimit(0.f, 1.f, pF(cv.ring) + modDests.dests[(int)vaRingDest(i)]);
        vap.wavefold    = juce::jlimit(0.f, 1.f, pF(cv.fold) + modDests.dests[(int)vaFoldDest(i)]);
        vaOscs[(size_t)i].setParams(vap);

        envBank.get(vaEnvIndex(i)).setParams(envParams(cv.env, 0.01f, 0.2f, 1.f, 0.3f));
    }

    // ── Karplus-Strong ────────────────────────────────────────────────────────
    for (int n = 0; n < currentKSCount; ++n)
    {
        const int i = activeKSIndices[(size_t)n];
        const auto& cks = pc.ks[(size_t)i];
        auto& ksp = cachedKSParams[(size_t)i];
        ksp.decay      = 0.99f + 0.0099f * juce::jlimit(0.f, 1.f, pF(cks.decay, 0.6f));
        ksp.brightness = juce::jlimit(0.f, 1.f, pF(cks.brightness, 0.5f));
        ksp.stiffness  = juce::jlimit(0.f, 0.95f, pF(cks.stiffness, 0.f));
        ksp.morph      = juce::jlimit(0.f, 1.f, pF(cks.morph, 0.f));
        ksp.position   = juce::jlimit(0.05f, 0.95f, pF(cks.position, 0.3f));
        float ksHz = pitchedHz * oscPitchMultiplier(cks.octave, cks.tune);
        ksOscs[(size_t)i].setFrequency(ksHz);
        envBank.get(ksEnvIndex(i)).setParams(envParams(cks.env, 0.001f, 0.1f, 0.f, 1.0f));
    }

    // ── Granular Oscillator Pool ──────────────────────────────────────────────
    for (int n = 0; n < currentGranCount; ++n)
    {
        const int i = activeGranIndices[(size_t)n];
        const auto& cg = pc.grain[(size_t)i];
        GranularOsc::Params gp;
        gp.triggerMode = static_cast<GranularOsc::TriggerMode>(pI(cg.triggerMode));
        gp.window      = static_cast<GranularOsc::Window>(pI(cg.window));
        gp.position    = juce::jlimit(0.f, 1.f,
            pF(cg.position, 0.5f) + modDests.dests[(int)ModDest::Gran_Position]);
        gp.sizeMs      = juce::jlimit(5.f, 1000.f,
            pF(cg.size, 120.f)
                * gliese::dsp::pitchRatioFromSemitones(modDests.dests[(int)ModDest::Gran_Size] * 36.f));
        gp.density     = juce::jlimit(0.1f, 1200.f,
            pF(cg.density, 35.f)
                * gliese::dsp::pitchRatioFromSemitones(modDests.dests[(int)ModDest::Gran_Density] * 48.f));
        gp.pitchSemis  = juce::jlimit(-48.f, 48.f,
            pF(cg.pitch) + modDests.dests[(int)ModDest::Gran_Pitch] * 24.f);
        gp.spray       = juce::jlimit(0.f, 1.f,
            pF(cg.spray, 0.18f) + modDests.dests[(int)ModDest::Gran_Spray]);
        gp.jitter      = pF(cg.jitter, 0.04f);
        gp.panSpread   = pF(cg.pan, 0.7f);
        gp.reverseProb = pF(cg.reverse);
        gp.scanSpeed   = pF(cg.scan);
        gp.freeze      = juce::jlimit(0.f, 1.f,
            pF(cg.freeze) + modDests.dests[(int)ModDest::Gran_Freeze]);
        gp.bodyType    = static_cast<GranularOsc::BodyType>(pI(cg.bodyType));
        gp.bodyRootSemis = pF(cg.bodyRoot);
        gp.bodySize    = juce::jlimit(0.f, 1.f,
            pF(cg.bodySize, 0.5f) + modDests.dests[(int)ModDest::Gran_BodySize]);
        gp.bodyDecay   = pF(cg.bodyDecay, 0.5f);
        gp.bodyDamping = juce::jlimit(0.f, 1.f,
            pF(cg.bodyDamp, 0.35f) + modDests.dests[(int)ModDest::Gran_BodyDamping]);
        gp.bodyBrightness = pF(cg.bodyBright, 0.6f);
        gp.bodyInharm  = juce::jlimit(0.f, 1.f,
            pF(cg.bodyInharm, 0.2f) + modDests.dests[(int)ModDest::Gran_BodyInharm]);
        gp.bodyExcite  = pF(cg.bodyExcite, 0.7f);
        gp.bodyMix     = juce::jlimit(0.f, 1.f,
            pF(cg.bodyMix) + modDests.dests[(int)ModDest::Gran_BodyMix]);
        gp.bodySpread  = pF(cg.bodySpread, 0.5f);
        gp.bodyModes   = pI(cg.bodyModes, 16);

        const float grainHz = pitchedHz * oscPitchMultiplier(cg.octave, cg.tune);
        granularOscs[(size_t)i].setFrequency(grainHz);
        granularOscs[(size_t)i].setParams(gp);
        envBank.get(granEnvIndex(i)).setParams(envParams(cg.env, 0.01f, 0.2f, 1.f, 0.4f));
    }

    // ── Terrain Oscillator Pool ──────────────────────────────────────────────
    for (int n = 0; n < currentTerrainCount; ++n)
    {
        const int i = activeTerrainIndices[(size_t)n];
        const auto& ct = pc.terrain[(size_t)i];
        TerrainOsc::Params tp;
        tp.pathMode    = static_cast<TerrainOsc::PathMode>(pI(ct.path));
        tp.quality     = static_cast<TerrainOsc::Quality>(pI(ct.quality, 1));
        tp.centerX     = pF(ct.centerX, 0.5f) + modDests.dests[(int) terrainCenterXDest(i)];
        tp.centerY     = pF(ct.centerY, 0.5f) + modDests.dests[(int) terrainCenterYDest(i)];
        tp.radiusX     = pF(ct.radiusX, 0.35f) + modDests.dests[(int) terrainRadiusXDest(i)];
        tp.radiusY     = pF(ct.radiusY, 0.35f) + modDests.dests[(int) terrainRadiusYDest(i)];
        tp.zoom        = pF(ct.zoom, 1.f);
        tp.rotation    = pF(ct.rotation);
        tp.pathSize    = pF(ct.pathSize, 1.f);
        tp.ratioX      = pF(ct.ratioX, 1.f) + modDests.dests[(int) terrainRatioXDest(i)];
        tp.ratioY      = pF(ct.ratioY, 1.5f) + modDests.dests[(int) terrainRatioYDest(i)];
        tp.scanRate    = pF(ct.scanRate, 1.f);
        tp.phaseOffset = pF(ct.phase);
        tp.spiral      = pF(ct.spiral);
        tp.drive       = pF(ct.drive, 1.5f) + modDests.dests[(int) terrainDriveDest(i)];
        tp.offset      = pF(ct.offset);
        tp.smoothing   = pF(ct.smoothing) + modDests.dests[(int) terrainSmoothingDest(i)];
        tp.morph       = pF(ct.morph) + modDests.dests[(int) terrainMorphDest(i)];
        tp.gain        = pF(ct.gain, 1.f);

        const float terrainHz = pitchedHz * oscPitchMultiplier(ct.octave, ct.tune);
        terrainOscs[(size_t)i].setFrequency(terrainHz);
        terrainOscs[(size_t)i].setParams(tp);
        envBank.get(terrainEnvIndex(i)).setParams(envParams(ct.env, 0.01f, 0.2f, 1.f, 0.3f));
    }

    // ── Per-engine pre-master filters ────────────────────────────────────────
    for (int i = 0; i < kNumEngineFilters; ++i)
    {
        if (!engineFilterSourceActive(i))
            continue;

        const auto& cef = pc.engineFilters[(size_t) i];
        const int modeParam = pI(cef.type, kEngineFilterNoneMode);
        if (modeParam >= kEngineFilterNoneMode)
            continue;

        SingleFilter::Params efp;
        const int modeIndex = juce::jlimit(0, (int) FilterMode::NumModes - 1,
                                           modeParam);
        efp.mode      = static_cast<FilterMode>(modeIndex);
        efp.cutoff    = juce::jlimit(20.f, (float)(sampleRate * 0.49),
            pF(cef.cutoff, 20000.f)
                * gliese::dsp::pitchRatioFromSemitones(modDests.dests[(int) engineFilterCutoffDest(i)] * 48.f));
        efp.resonance = juce::jlimit(0.f, 1.f,
            pF(cef.resonance) + modDests.dests[(int) engineFilterResDest(i)]);
        efp.drive     = juce::jlimit(0.f, 1.f,
            pF(cef.drive) + modDests.dests[(int) engineFilterDriveDest(i)]);
        efp.morph     = juce::jlimit(0.f, 1.f,
            pF(cef.morph, 0.5f) + modDests.dests[(int) engineFilterMorphDest(i)]);
        efp.saturation = juce::jlimit(0.f, 1.f, pF(cef.saturation, 0.35f));
        efp.bassComp  = juce::jlimit(0.f, 1.f, pF(cef.bassComp));
        efp.mix       = juce::jlimit(0.f, 1.f,
            pF(cef.mix, 1.f) + modDests.dests[(int) engineFilterMixDest(i)]);
        efp.oversample = pI(cef.oversample, 0);
        efp.envAmount = pF(cef.envAmt);
        efp.lfoAmount = pF(cef.lfoAmt);
        engineFilterL[(size_t) i].setParams(efp);
        engineFilterR[(size_t) i].setParams(efp);
    }

    // ── Filters ───────────────────────────────────────────────────────────────
    const auto& cfa = pc.filterA;
    SingleFilter::Params fpA;
    fpA.mode      = static_cast<FilterMode>(pI(cfa.type));
    fpA.cutoff    = juce::jlimit(20.f, (float)(sampleRate * 0.49),
                                 pF(cfa.cutoff, 20000.f)
                                    * gliese::dsp::pitchRatioFromSemitones(modDests.dests[(int)ModDest::FilterA_Cutoff] * 48.f));
    fpA.resonance = juce::jlimit(0.f, 1.f, pF(cfa.resonance) + modDests.dests[(int)ModDest::FilterA_Resonance]);
    fpA.drive     = juce::jlimit(0.f, 1.f, pF(cfa.drive)     + modDests.dests[(int)ModDest::FilterA_Drive]);
    fpA.morph     = pF(cfa.morph, 0.5f);
    fpA.saturation = pF(cfa.saturation, 0.35f);
    fpA.bassComp  = pF(cfa.bassComp);
    fpA.outputGainDb = pF(cfa.gain);
    fpA.mix       = pF(cfa.mix, 1.f);
    fpA.oversample = pI(cfa.oversample, 0);
    fpA.envAmount = pF(cfa.envAmt);
    fpA.lfoAmount = pF(cfa.lfoAmt);
    auto envA = envParams(cfa.env, 0.05f, 0.3f, 0.f, 0.2f);
    dualFilter.getEnvA().setParams(envA);
    envBank.get(ENV_FILTER_A).setParams(envA);

    const auto& cfb = pc.filterB;
    SingleFilter::Params fpB;
    fpB.mode      = static_cast<FilterMode>(pI(cfb.type));
    fpB.cutoff    = juce::jlimit(20.f, (float)(sampleRate * 0.49),
                                 pF(cfb.cutoff, 20000.f)
                                    * gliese::dsp::pitchRatioFromSemitones(modDests.dests[(int)ModDest::FilterB_Cutoff] * 48.f));
    fpB.resonance = juce::jlimit(0.f, 1.f, pF(cfb.resonance) + modDests.dests[(int)ModDest::FilterB_Resonance]);
    fpB.drive     = juce::jlimit(0.f, 1.f, pF(cfb.drive)     + modDests.dests[(int)ModDest::FilterB_Drive]);
    fpB.morph     = pF(cfb.morph, 0.5f);
    fpB.saturation = pF(cfb.saturation, 0.35f);
    fpB.bassComp  = pF(cfb.bassComp);
    fpB.outputGainDb = pF(cfb.gain);
    fpB.mix       = pF(cfb.mix, 1.f);
    fpB.oversample = pI(cfb.oversample, 0);
    fpB.envAmount = pF(cfb.envAmt);
    fpB.lfoAmount = pF(cfb.lfoAmt);
    auto envB = envParams(cfb.env, 0.05f, 0.3f, 0.f, 0.2f);
    dualFilter.getEnvB().setParams(envB);
    envBank.get(ENV_FILTER_B).setParams(envB);

    dualFilter.setFilterA(fpA);
    dualFilter.setFilterB(fpB);
    dualFilter.setRouting(static_cast<FilterRouting>(pI(pc.filterRouting)));

    envBank.get(ENV_MASTER).setParams(envParams(pc.master.env, 0.01f, 0.2f, 0.8f, 0.3f));
}

//──────────────────────────────────────────────────────────────────────────────
void SynthVoice::buildModSources()
{
    auto maxEnv = [this](int first, int count)
    {
        float value = 0.f;
        for (int i = 0; i < count; ++i)
            value = juce::jmax(value, envBank.get(first + i).getCurrentValue());
        return value;
    };

    modSources.sources[(int)ModSource::LFO1] = lfo.getValue(0);
    modSources.sources[(int)ModSource::LFO2] = lfo.getValue(1);
    modSources.sources[(int)ModSource::LFO3] = lfo.getValue(2);
    modSources.sources[(int)ModSource::LFO4] = lfo.getValue(3);

    modSources.sources[(int)ModSource::Env_Master]  = envBank.get(ENV_MASTER).getCurrentValue();
    modSources.sources[(int)ModSource::Env_3D]      = maxEnv((int)ENV_3D1, kNum3DOscs);
    modSources.sources[(int)ModSource::Env_FM]      = envBank.get(ENV_FM).getCurrentValue();
    modSources.sources[(int)ModSource::Env_WT]      = maxEnv((int)ENV_WT1, kNumWTOscs);
    modSources.sources[(int)ModSource::Env_VA]      = maxEnv((int)ENV_VA1, kNumVAOscs);
    modSources.sources[(int)ModSource::Env_FilterA] = envBank.get(ENV_FILTER_A).getCurrentValue();
    modSources.sources[(int)ModSource::Env_FilterB] = envBank.get(ENV_FILTER_B).getCurrentValue();

    modSources.sources[(int)ModSource::Velocity]     = velocity;
    modSources.sources[(int)ModSource::Aftertouch]   = midiCCs[(int)ModSource::Aftertouch];
    modSources.sources[(int)ModSource::ModWheel]     = midiCCs[(int)ModSource::ModWheel]
                                                       * pF(pc.master.mwRange, 0.5f);
    modSources.sources[(int)ModSource::PitchBend]    = midiCCs[(int)ModSource::PitchBend];
    modSources.sources[(int)ModSource::CC74]         = midiCCs[(int)ModSource::CC74];
    modSources.sources[(int)ModSource::ChanPressure] = midiCCs[(int)ModSource::ChanPressure];
}

//──────────────────────────────────────────────────────────────────────────────
void SynthVoice::renderBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                              juce::AudioProcessorValueTreeState& /*apvts*/,
                              const ModValues& globalModDests)
{
    if (!isActive()) return;
    static constexpr float kSilent = 1.0e-5f;

    if (!pc.built.load(std::memory_order_acquire)) return;

    for (int i = 0; i < LFOEngine::kNumLFOs; ++i)
    {
        const auto& lc = pc.lfos[(size_t)i];
        LFOEngine::LFOParams lp;
        lp.shape   = static_cast<LFOEngine::Shape>(pI(lc.shape));
        lp.rate    = juce::jlimit(0.01f, 100.f,
                                  pF(lc.rate, 1.f) + modDests.dests[(int)ModDest::LFO1_Rate + i]);
        lp.sync    = pB(lc.sync, false);
        lp.syncRate = juce::jlimit(0.f, 10.f,
                                   pF(lc.syncRate, 5.f) + modDests.dests[(int)ModDest::LFO1_SyncRate + i] * 10.f);
        lp.amount  = juce::jlimit(0.f, 1.f,
                                  pF(lc.amount, 0.5f) + modDests.dests[(int)ModDest::LFO1_Amount + i]);
        lp.phase   = pF(lc.phase);
        lp.retrig  = pI(lc.retrig) != 0;
        lfo.setParams(i, lp);
    }
    // Prime the control values at the first sample. Subsequent values are
    // advanced inside the render loop so audio-rate destinations do not step
    // once per host block.
    lfo.processSample();

    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const auto& mc = pc.mods[(size_t)i];
        ModSlot slot;
        slot.source  = static_cast<ModSource>(pI(mc.source));
        slot.dest    = static_cast<ModDest>  (pI(mc.dest));
        slot.amount  = pF(mc.amount);
        slot.bipolar = pB(mc.bipolar);
        modMatrix.setSlot(i, slot);
    }

    for (int i = 0; i < 8; ++i)
        modSources.sources[(int)ModSource::Macro1 + i] = pF(pc.macros[(size_t)i]);

    buildModSources();
    modMatrix.process(modSources, modDests);
    for (int i = 0; i < (int) ModDest::NumDests; ++i)
        modDests.dests[i] = juce::jlimit(-4.0f, 4.0f, modDests.dests[i] + globalModDests.dests[i]);
    syncParams();

    // ── Level / enable flags ──────────────────────────────────────────────────
    float fmLevelA   = juce::jlimit(0.f, 1.f, pF(pc.fm.level));
    float fmLevelB   = juce::jlimit(0.f, 1.f, pF(pc.fmB.level));

    std::array<float, kNum3DOscs> osc3dLevels {};
    for (int i = 0; i < kNum3DOscs; ++i)
        osc3dLevels[(size_t)i] = juce::jlimit(0.f, 1.f,
            pF(pc.osc3d[(size_t)i].level, 0.8f) + modDests.dests[(int)ModDest::Osc3D_Level]);

    std::array<float, kNumWTOscs> wtLevels {};
    for (int i = 0; i < kNumWTOscs; ++i)
        wtLevels[(size_t)i] = juce::jlimit(0.f, 1.f,
            pF(pc.wt[(size_t)i].level) + modDests.dests[(int)ModDest::WT_Level]);

    std::array<float, kNumKSOscs> ksLevels {};
    for (int i = 0; i < kNumKSOscs; ++i)
        ksLevels[(size_t)i] = juce::jlimit(0.f, 1.f, pF(pc.ks[(size_t)i].level));

    std::array<float, kNumGranOscs> grainLevels {};
    for (int i = 0; i < kNumGranOscs; ++i)
        grainLevels[(size_t)i] = juce::jlimit(0.f, 1.f,
            pF(pc.grain[(size_t)i].level) + modDests.dests[(int)ModDest::Gran_Level]);

    std::array<float, kNumTerrainOscs> terrainLevels {};
    for (int i = 0; i < kNumTerrainOscs; ++i)
        terrainLevels[(size_t)i] = juce::jlimit(0.f, 1.f, pF(pc.terrain[(size_t)i].level));

    std::array<float, kNumVAOscs> vaLevels {};
    for (int i = 0; i < kNumVAOscs; ++i)
        vaLevels[(size_t)i] = juce::jlimit(0.f, 1.f,
            pF(pc.va[(size_t)i].level) + modDests.dests[(int)vaLevelDest(i)]);

    float masterVol  = juce::jlimit(0.f, 1.f,
        pF(pc.master.volume, 0.8f) + modDests.dests[(int)ModDest::Master_Volume]);
    float masterPan  = juce::jlimit(-1.f, 1.f,
        pF(pc.master.pan) + modDests.dests[(int)ModDest::Master_Pan]);
    masterVolumeSmooth.setTargetValue(masterVol);
    masterPanSmooth.setTargetValue(masterPan);
    fmLevelSmooth[0].setTargetValue(pB(pc.fm.on) ? fmLevelA : 0.f);
    fmLevelSmooth[1].setTargetValue(pB(pc.fmB.on) ? fmLevelB : 0.f);
    for (int i = 0; i < kNum3DOscs; ++i)
        osc3dLevelSmooth[(size_t)i].setTargetValue(pB(pc.osc3d[(size_t)i].on) ? osc3dLevels[(size_t)i] : 0.f);
    for (int i = 0; i < kNumWTOscs; ++i)
        wtLevelSmooth[(size_t)i].setTargetValue(pB(pc.wt[(size_t)i].on) ? wtLevels[(size_t)i] : 0.f);
    for (int i = 0; i < kNumKSOscs; ++i)
        ksLevelSmooth[(size_t)i].setTargetValue(pB(pc.ks[(size_t)i].on) ? ksLevels[(size_t)i] : 0.f);
    for (int i = 0; i < kNumGranOscs; ++i)
        grainLevelSmooth[(size_t)i].setTargetValue(pB(pc.grain[(size_t)i].on) ? grainLevels[(size_t)i] : 0.f);
    for (int i = 0; i < kNumTerrainOscs; ++i)
        terrainLevelSmooth[(size_t)i].setTargetValue(pB(pc.terrain[(size_t)i].on) ? terrainLevels[(size_t)i] : 0.f);
    for (int i = 0; i < kNumVAOscs; ++i)
        vaLevelSmooth[(size_t)i].setTargetValue(pB(pc.va[(size_t)i].on) ? vaLevels[(size_t)i] : 0.f);

    if (!smoothingInitialized)
    {
        auto snapToTarget = [](auto& smooth) {
            smooth.setCurrentAndTargetValue(smooth.getTargetValue());
        };
        snapToTarget(masterVolumeSmooth);
        snapToTarget(masterPanSmooth);
        for (auto& smooth : fmLevelSmooth) snapToTarget(smooth);
        for (auto& smooth : osc3dLevelSmooth) snapToTarget(smooth);
        for (auto& smooth : wtLevelSmooth) snapToTarget(smooth);
        for (auto& smooth : ksLevelSmooth) snapToTarget(smooth);
        for (auto& smooth : grainLevelSmooth) snapToTarget(smooth);
        for (auto& smooth : terrainLevelSmooth) snapToTarget(smooth);
        for (auto& smooth : vaLevelSmooth) snapToTarget(smooth);
        smoothingInitialized = true;
    }

    float lfoA = lfo.getValue(0);
    float lfoB = lfo.getValue(1);

    const bool useFMA = currentBlockAIsFM
                     && (fmLevelSmooth[0].getTargetValue() > kSilent
                         || fmLevelSmooth[0].getCurrentValue() > kSilent);
    const bool useFMB = currentBlockBIsFM
                     && (fmLevelSmooth[1].getTargetValue() > kSilent
                         || fmLevelSmooth[1].getCurrentValue() > kSilent);
    const bool useFM  = useFMA || useFMB;

    std::array<bool, kNum3DOscs> use3D {};
    for (int i = 0; i < current3dCount; ++i)
    {
        const int idx = active3dIndices[(size_t)i];
        use3D[(size_t)idx] = osc3dLevelSmooth[(size_t)idx].getTargetValue() > kSilent
                          || osc3dLevelSmooth[(size_t)idx].getCurrentValue() > kSilent;
    }

    std::array<bool, kNumWTOscs> useWT {};
    for (int i = 0; i < currentWTCount; ++i)
    {
        const int idx = activeWTIndices[(size_t)i];
        useWT[(size_t)idx] = wtLevelSmooth[(size_t)idx].getTargetValue() > kSilent
                          || wtLevelSmooth[(size_t)idx].getCurrentValue() > kSilent;
    }

    std::array<bool, kNumKSOscs> useKS {};
    for (int i = 0; i < currentKSCount; ++i)
    {
        const int idx = activeKSIndices[(size_t)i];
        useKS[(size_t)idx] = ksLevelSmooth[(size_t)idx].getTargetValue() > kSilent
                          || ksLevelSmooth[(size_t)idx].getCurrentValue() > kSilent;
    }

    std::array<bool, kNumGranOscs> useGran {};
    for (int i = 0; i < currentGranCount; ++i)
    {
        const int idx = activeGranIndices[(size_t)i];
        useGran[(size_t)idx] = grainLevelSmooth[(size_t)idx].getTargetValue() > kSilent
                            || grainLevelSmooth[(size_t)idx].getCurrentValue() > kSilent;
    }

    std::array<bool, kNumTerrainOscs> useTerrain {};
    for (int i = 0; i < currentTerrainCount; ++i)
    {
        const int idx = activeTerrainIndices[(size_t)i];
        useTerrain[(size_t)idx] = terrainLevelSmooth[(size_t)idx].getTargetValue() > kSilent
                               || terrainLevelSmooth[(size_t)idx].getCurrentValue() > kSilent;
    }

    std::array<bool, kNumVAOscs> useVA {};
    for (int i = 0; i < currentVACount; ++i)
    {
        const int idx = activeVAIndices[(size_t)i];
        useVA[(size_t)idx] = vaLevelSmooth[(size_t)idx].getTargetValue() > kSilent
                          || vaLevelSmooth[(size_t)idx].getCurrentValue() > kSilent;
    }

    const float osc3dNorm = current3dCount > 0
        ? 1.f / std::sqrt((float)current3dCount) : 1.f;
    const bool  dualFM     = currentBlockAIsFM && currentBlockBIsFM;
    const int   activeFMCount = (useFMA ? 1 : 0) + (useFMB ? 1 : 0);
    const float fmScale    = activeFMCount > 0 ? 1.f / std::sqrt((float)activeFMCount) : 1.f;
    const float wtNorm     = currentWTCount > 0
        ? 1.f / std::sqrt((float)currentWTCount) : 1.f;
    const float ksNorm     = currentKSCount > 0
        ? 1.f / (float)currentKSCount : 1.f;
    const float grainNorm  = currentGranCount > 0
        ? 1.f / std::sqrt((float)currentGranCount) : 1.f;
    const float terrainNorm = currentTerrainCount > 0
        ? 1.f / std::sqrt((float)currentTerrainCount) : 1.f;

    std::array<bool, kNumEngineFilters> engineFilterOn {};
    auto anyActiveSlot = [](const auto& useFlags, const auto& activeIndices, int count) {
        for (int n = 0; n < count; ++n)
            if (useFlags[(size_t) activeIndices[(size_t) n]])
                return true;
        return false;
    };
    auto engineFilterHasSource = [&](int filterIndex) {
        switch (filterIndex)
        {
            case EngineFilter3D:      return anyActiveSlot(use3D, active3dIndices, current3dCount);
            case EngineFilterFM:      return useFM;
            case EngineFilterWT:      return anyActiveSlot(useWT, activeWTIndices, currentWTCount);
            case EngineFilterVA:      return anyActiveSlot(useVA, activeVAIndices, currentVACount);
            case EngineFilterKS:      return anyActiveSlot(useKS, activeKSIndices, currentKSCount);
            case EngineFilterGran:    return anyActiveSlot(useGran, activeGranIndices, currentGranCount);
            case EngineFilterTerrain: return anyActiveSlot(useTerrain, activeTerrainIndices, currentTerrainCount);
            default:                  return false;
        }
    };
    for (int i = 0; i < kNumEngineFilters; ++i)
    {
        const auto& ef = pc.engineFilters[(size_t) i];
        engineFilterOn[(size_t) i] = engineFilterHasSource(i)
                                  && pB(ef.on, false)
                                  && pI(ef.type, kEngineFilterNoneMode) < kEngineFilterNoneMode;
    }

    auto* leftCh  = buffer.getWritePointer(0, startSample);
    auto* rightCh = buffer.getWritePointer(1, startSample);

    ++age;

    for (int s = 0; s < numSamples; ++s)
    {
        if (s > 0)
        {
            lfo.processSample();
            lfoA = lfo.getValue(0);
            lfoB = lfo.getValue(1);
        }

        float masterEnv = envBank.processAndGet(ENV_MASTER);
        const float smoothedMasterVolume = masterVolumeSmooth.getNextValue();
        const float smoothedMasterPan = masterPanSmooth.getNextValue();
        const float panGainL = smoothedMasterVolume * std::sqrt(0.5f - 0.5f * smoothedMasterPan);
        const float panGainR = smoothedMasterVolume * std::sqrt(0.5f + 0.5f * smoothedMasterPan);
        const float smoothedFmLevelA = fmLevelSmooth[0].getNextValue();
        const float smoothedFmLevelB = fmLevelSmooth[1].getNextValue();

        if (masterEnv < kSilent)
        {
            // Drain envelopes while tailing off
            for (int i = 0; i < kNum3DOscs; ++i)
                envBank.processAndGet(osc3dEnvIndex(i));
            envBank.processAndGet(ENV_FM);
            envBank.processAndGet(ENV_FM2);
            for (int i = 0; i < kNumWTOscs; ++i)
                envBank.processAndGet(wtEnvIndex(i));
            for (int i = 0; i < kNumKSOscs; ++i)
                envBank.processAndGet(ksEnvIndex(i));
            for (int i = 0; i < kNumGranOscs; ++i)
                envBank.processAndGet(granEnvIndex(i));
            for (int i = 0; i < kNumTerrainOscs; ++i)
                envBank.processAndGet(terrainEnvIndex(i));
            for (int i = 0; i < kNumVAOscs; ++i)
                envBank.processAndGet(vaEnvIndex(i));
            envBank.processAndGet(ENV_FILTER_A);
            envBank.processAndGet(ENV_FILTER_B);
            fm.advanceEnvelopes();
            if (dualFM) fm2.advanceEnvelopes();
            if (!envBank.anyActive()) { midiNote = -1; break; }
            continue;
        }

        float envFMA = useFMA ? envBank.processAndGet(ENV_FM) : 0.f;
        float envFMB = useFMB ? envBank.processAndGet(ENV_FM2) : 0.f;
        float env3D[kNum3DOscs] {};
        float envWT[kNumWTOscs] {};
        float envKS[kNumKSOscs] {};
        float envGran[kNumGranOscs] {};
        float envTerrain[kNumTerrainOscs] {};
        for (int n = 0; n < current3dCount; ++n)
        {
            const int i = active3dIndices[(size_t)n];
            env3D[i] = use3D[(size_t)i] ? envBank.processAndGet(osc3dEnvIndex(i)) : 0.f;
        }
        for (int n = 0; n < currentWTCount; ++n)
        {
            const int i = activeWTIndices[(size_t)n];
            envWT[i] = useWT[(size_t)i] ? envBank.processAndGet(wtEnvIndex(i)) : 0.f;
        }
        for (int n = 0; n < currentKSCount; ++n)
        {
            const int i = activeKSIndices[(size_t)n];
            envKS[i] = useKS[(size_t)i] ? envBank.processAndGet(ksEnvIndex(i)) : 0.f;
        }
        for (int n = 0; n < currentGranCount; ++n)
        {
            const int i = activeGranIndices[(size_t)n];
            envGran[i] = useGran[(size_t)i] ? envBank.processAndGet(granEnvIndex(i)) : 0.f;
        }
        for (int n = 0; n < currentTerrainCount; ++n)
        {
            const int i = activeTerrainIndices[(size_t)n];
            envTerrain[i] = useTerrain[(size_t)i] ? envBank.processAndGet(terrainEnvIndex(i)) : 0.f;
        }
        float envVA[kNumVAOscs] {};
        for (int n = 0; n < currentVACount; ++n)
        {
            const int i = activeVAIndices[(size_t)n];
            envVA[i] = useVA[(size_t)i] ? envBank.processAndGet(vaEnvIndex(i)) : 0.f;
        }
        envBank.processAndGet(ENV_FILTER_A);
        envBank.processAndGet(ENV_FILTER_B);

        float sig3D = 0.f;
        float sigFM = 0.f;
        float sigKS = 0.f;
        float sigWTL = 0.f, sigWTR = 0.f;
        float sigGranL = 0.f, sigGranR = 0.f;
        float sigTerrain = 0.f;
        float sigVAL = 0.f, sigVAR = 0.f;
        float env3DGroup = 0.f;
        float envFMGroup = 0.f;
        float envKSGroup = 0.f;
        float envWTGroup = 0.f;
        float envGranGroup = 0.f;
        float envTerrainGroup = 0.f;
        float envVAGroup = 0.f;

        // 3D instances
        if (current3dCount > 0)
        {
            float sum = 0.f;
            for (int n = 0; n < current3dCount; ++n)
            {
                const int i = active3dIndices[(size_t)n];
                if (use3D[(size_t)i] && env3D[i] > kSilent)
                {
                    sum += osc3dArray[(size_t)i].processSample() * env3D[i]
                         * osc3dLevelSmooth[(size_t)i].getNextValue();
                    env3DGroup = juce::jmax(env3DGroup, env3D[i]);
                }
            }
            sig3D = sum * osc3dNorm;
        }

        // FM engines
        if (useFM)
        {
            if (useFMA)
            {
                if (envFMA > kSilent)
                {
                    sigFM += fm.processSample() * fmScale * envFMA * smoothedFmLevelA;
                    envFMGroup = juce::jmax(envFMGroup, envFMA);
                }
                else
                    fm.advanceEnvelopes();
            }

            if (useFMB)
            {
                if (envFMB > kSilent)
                {
                    sigFM += fm2.processSample() * fmScale * envFMB * smoothedFmLevelB;
                    envFMGroup = juce::jmax(envFMGroup, envFMB);
                }
                else
                    fm2.advanceEnvelopes();
            }
        }

        // KS pool
        for (int n = 0; n < currentKSCount; ++n)
        {
            const int i = activeKSIndices[(size_t)n];
            if (useKS[(size_t)i] && (ksOscs[(size_t)i].isActive() || envKS[i] > kSilent))
            {
                sigKS += ksOscs[(size_t)i].processSample(cachedKSParams[(size_t)i])
                       * envKS[i] * ksLevelSmooth[(size_t)i].getNextValue() * ksNorm;
                envKSGroup = juce::jmax(envKSGroup, envKS[i]);
            }
        }

        // WT pool (stereo)
        if (currentWTCount > 0)
        {
            for (int n = 0; n < currentWTCount; ++n)
            {
                const int i = activeWTIndices[(size_t)n];
                if (useWT[(size_t)i] && envWT[i] > kSilent)
                {
                    auto [wtL, wtR] = wtOscs[(size_t)i].processSample();
                    const float level = wtLevelSmooth[(size_t)i].getNextValue();
                    sigWTL += wtL * envWT[i] * level * wtNorm;
                    sigWTR += wtR * envWT[i] * level * wtNorm;
                    envWTGroup = juce::jmax(envWTGroup, envWT[i]);
                }
            }
        }

        // Granular pool (stereo cloud)
        if (currentGranCount > 0)
        {
            for (int n = 0; n < currentGranCount; ++n)
            {
                const int i = activeGranIndices[(size_t)n];
                if (useGran[(size_t)i] && envGran[i] > kSilent)
                {
                    auto [gL, gR] = granularOscs[(size_t)i].processSample();
                    const float level = grainLevelSmooth[(size_t)i].getNextValue();
                    sigGranL += gL * envGran[i] * level * grainNorm;
                    sigGranR += gR * envGran[i] * level * grainNorm;
                    envGranGroup = juce::jmax(envGranGroup, envGran[i]);
                }
            }
        }

        // Terrain pool (mono wave-terrain surfaces)
        for (int n = 0; n < currentTerrainCount; ++n)
        {
            const int i = activeTerrainIndices[(size_t)n];
            if (useTerrain[(size_t)i] && envTerrain[i] > kSilent)
            {
                const float t = terrainOscs[(size_t)i].processSample()
                              * envTerrain[i] * terrainLevelSmooth[(size_t)i].getNextValue()
                              * terrainNorm;
                sigTerrain += t;
                envTerrainGroup = juce::jmax(envTerrainGroup, envTerrain[i]);
            }
        }

        // VA instances (stereo, each independent)
        for (int n = 0; n < currentVACount; ++n)
        {
            const int i = activeVAIndices[(size_t)n];
            if (useVA[(size_t)i] && envVA[i] > kSilent)
            {
                auto [vaL, vaR] = vaOscs[(size_t)i].processSample();
                const float level = vaLevelSmooth[(size_t)i].getNextValue();
                sigVAL += vaL * envVA[i] * level;
                sigVAR += vaR * envVA[i] * level;
                envVAGroup = juce::jmax(envVAGroup, envVA[i]);
            }
        }

        auto processEngineFilter = [&](int filterIndex, float& left, float& right,
                                       float envValue, float lfoValue)
        {
            if (!engineFilterOn[(size_t) filterIndex])
                return;
            if (std::abs(left) < kSilent && std::abs(right) < kSilent && envValue < kSilent)
                return;

            left = engineFilterL[(size_t) filterIndex].processSample(left, envValue, lfoValue);
            right = engineFilterR[(size_t) filterIndex].processSample(right, envValue, lfoValue);
        };

        float sig3DL = sig3D, sig3DR = sig3D;
        float sigFML = sigFM, sigFMR = sigFM;
        float sigKSL = sigKS, sigKSR = sigKS;
        float sigTerrainL = sigTerrain, sigTerrainR = sigTerrain;

        processEngineFilter(EngineFilter3D, sig3DL, sig3DR, env3DGroup, lfoA);
        processEngineFilter(EngineFilterFM, sigFML, sigFMR, envFMGroup, lfoA);
        processEngineFilter(EngineFilterWT, sigWTL, sigWTR, envWTGroup, lfoA);
        processEngineFilter(EngineFilterVA, sigVAL, sigVAR, envVAGroup, lfoA);
        processEngineFilter(EngineFilterKS, sigKSL, sigKSR, envKSGroup, lfoB);
        processEngineFilter(EngineFilterGran, sigGranL, sigGranR, envGranGroup, lfoB);
        processEngineFilter(EngineFilterTerrain, sigTerrainL, sigTerrainR, envTerrainGroup, lfoB);

        float sigL = sig3DL + sigFML + sigKSL + sigWTL + sigGranL + sigTerrainL + sigVAL;
        float sigR = sig3DR + sigFMR + sigKSR + sigWTR + sigGranR + sigTerrainR + sigVAR;

        auto [fL, fR] = dualFilter.processSample(sigL, sigR, lfoA, lfoB);

        leftCh[s]  += fL * masterEnv * panGainL;
        rightCh[s] += fR * masterEnv * panGainR;
    }
}
