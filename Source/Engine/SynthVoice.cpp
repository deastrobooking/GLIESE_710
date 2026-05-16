#include "SynthVoice.h"

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

// VA param prefix: index 0 → "osc_va_", 1 → "osc_va2_", 4 → "osc_va5_", etc.
juce::String vaPrefix(int index)
{
    return index == 0 ? juce::String("osc_va_")
                      : juce::String("osc_va") + juce::String(index + 1) + "_";
}

// Map VA instance index to envelope slot (clamp at 4 available slots)
EnvIndex vaEnvIndex(int index)
{
    static constexpr EnvIndex envs[] = { ENV_VA, ENV_VA2, ENV_VA3, ENV_VA4 };
    return envs[juce::jlimit(0, 3, index)];
}

ModDest vaShapeDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Shape, ModDest::VA2_Shape, ModDest::VA3_Shape, ModDest::VA4_Shape };
    return d[juce::jlimit(0, 3, index)];
}
ModDest vaPwDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_PW, ModDest::VA2_PW, ModDest::VA3_PW, ModDest::VA4_PW };
    return d[juce::jlimit(0, 3, index)];
}
ModDest vaDetuneDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Detune, ModDest::VA2_Detune, ModDest::VA3_Detune, ModDest::VA4_Detune };
    return d[juce::jlimit(0, 3, index)];
}
ModDest vaSpreadDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Spread, ModDest::VA2_Spread, ModDest::VA3_Spread, ModDest::VA4_Spread };
    return d[juce::jlimit(0, 3, index)];
}
ModDest vaLevelDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Level, ModDest::VA2_Level, ModDest::VA3_Level, ModDest::VA4_Level };
    return d[juce::jlimit(0, 3, index)];
}
ModDest vaRingDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Ring, ModDest::VA2_Ring, ModDest::VA3_Ring, ModDest::VA4_Ring };
    return d[juce::jlimit(0, 3, index)];
}
ModDest vaFoldDest(int index)
{
    static constexpr ModDest d[] = {
        ModDest::VA_Fold, ModDest::VA2_Fold, ModDest::VA3_Fold, ModDest::VA4_Fold };
    return d[juce::jlimit(0, 3, index)];
}
} // namespace

float SynthVoice::midiToHz(int note, float pitchBendSemitones)
{
    return 440.f * std::pow(2.f, ((float)(note - 69) + pitchBendSemitones) / 12.f);
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
    envBank.prepare(sr);
    lfo.prepare(sr);
    dualFilter.prepare(sr);
}

void SynthVoice::reset()
{
    midiNote = -1;
    age      = 0;
    for (auto& osc : osc3dArray) osc.reset();
    fm.reset();
    fm2.reset();
    for (auto& wt : wtOscs) wt.reset();
    for (auto& va : vaOscs) va.reset();
    for (auto& ks : ksOscs) ks.reset();
    envBank.reset();
    lfo.reset();
    dualFilter.reset();
}

void SynthVoice::noteOn(int note, float vel)
{
    midiNote = note;
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
    for (auto& va : vaOscs) va.setFrequency(hz);
    for (auto& ks : ksOscs) {
        ks.setFrequency(hz);
        ks.pluck(velocity, cachedKSParams);
    }

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
    dualFilter.noteOff();
}

void SynthVoice::setHostBPM(double bpm)  { lfo.setHostBPM(bpm); }

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

    pc.osc3d.field     = get("osc_3d_field");
    pc.osc3d.path      = get("osc_3d_path");
    pc.osc3d.radius    = get("osc_3d_radius");
    pc.osc3d.depth     = get("osc_3d_depth");
    pc.osc3d.warp      = get("osc_3d_warp");
    pc.osc3d.rotation  = get("osc_3d_rotation");
    pc.osc3d.chaos     = get("osc_3d_chaos");
    pc.osc3d.morph     = get("osc_3d_morph");
    pc.osc3d.fieldFreq = get("osc_3d_field_freq");
    pc.osc3d.zRatio    = get("osc_3d_z_ratio");
    pc.osc3d.fold      = get("osc_3d_fold");
    pc.osc3d.drive     = get("osc_3d_drive");
    pc.osc3d.on        = get("osc_3d_on");
    pc.osc3d.level     = get("osc_3d_level");
    pc.osc3d.octave    = get("osc_3d_octave");
    cacheEnv(pc.osc3d.env, "osc_3d_");

    pc.fm.on        = get("osc_fm_on");
    pc.fm.algorithm = get("osc_fm_algorithm");
    pc.fm.level     = get("osc_fm_level");
    pc.fm.octave    = get("osc_fm_octave");
    cacheEnv(pc.fm.env, "osc_fm_");

    pc.wt.on     = get("osc_wt_on");
    pc.wt.table  = get("osc_wt_table");
    pc.wt.pos    = get("osc_wt_pos");
    pc.wt.level  = get("osc_wt_level");
    pc.wt.octave = get("osc_wt_octave");
    cacheEnv(pc.wt.env, "osc_wt_");

    for (int i = 0; i < kNumVAOscs; ++i)
    {
        juce::String pfx = vaPrefix(i);
        auto& v = pc.va[(size_t)i];
        v.on     = get(pfx + "on");
        v.shape  = get(pfx + "shape");
        v.pw     = get(pfx + "pw");
        v.detune = get(pfx + "detune");
        v.unison = get(pfx + "unison");
        v.spread = get(pfx + "spread");
        v.ring   = get(pfx + "ring");
        v.fold   = get(pfx + "fold");
        v.level  = get(pfx + "level");
        v.octave = get(pfx + "octave");
        cacheEnv(v.env, pfx);
    }

    pc.ks.on         = get("osc_ks_on");
    pc.ks.level      = get("osc_ks_level");
    pc.ks.octave     = get("osc_ks_octave");
    pc.ks.decay      = get("osc_ks_decay");
    pc.ks.brightness = get("osc_ks_brightness");
    pc.ks.stiffness  = get("osc_ks_stiffness");
    pc.ks.morph      = get("osc_ks_morph");
    pc.ks.position   = get("osc_ks_position");
    cacheEnv(pc.ks.env, "osc_ks_");

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

    auto cacheFilter = [&](ParamCache::FilterPtrs& f, const juce::String& pfx) {
        f.type      = get(pfx + "type");
        f.cutoff    = get(pfx + "cutoff");
        f.resonance = get(pfx + "resonance");
        f.drive     = get(pfx + "drive");
        f.envAmt    = get(pfx + "env_amt");
        f.lfoAmt    = get(pfx + "lfo_amt");
        cacheEnv(f.env, pfx);
    };
    cacheFilter(pc.filterA, "filter_a_");
    cacheFilter(pc.filterB, "filter_b_");
    pc.filterRouting = get("filter_routing");

    for (int i = 0; i < 10; ++i)
    {
        juce::String pfx = "op" + juce::String(i) + "_";
        auto& o  = pc.ops[(size_t)i];
        o.coarse   = get(pfx + "coarse");
        o.fine     = get(pfx + "fine");
        o.level    = get(pfx + "level");
        o.feedback = get(pfx + "feedback");
        o.fixed    = get(pfx + "fixed");
        o.wave     = get(pfx + "wave");
        o.vel      = get(pfx + "vel");
        o.key      = get(pfx + "key");
        o.adsrA    = get(pfx + "adsr_a");
        o.adsrD    = get(pfx + "adsr_d");
        o.adsrS    = get(pfx + "adsr_s");
        o.adsrR    = get(pfx + "adsr_r");
    }

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

    pc.built = true;
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
    currentBlockAIsFM = pI(pc.slotCfg.blockAMode, 1) == 1;
    currentBlockBIsFM = pI(pc.slotCfg.blockBMode, 0) == 1;

    current3dCount = 0;
    currentVACount = 0;
    currentWTCount = 0;
    currentKSCount = 0;

    for (int b = 0; b < 2; ++b)
    {
        bool isFM = (b == 0) ? currentBlockAIsFM : currentBlockBIsFM;
        if (isFM) continue;
        for (int s = 0; s < 4; ++s)
        {
            int t = pI(pc.slotCfg.slotTypes[b][s], 0);
            if      (t == 1 && current3dCount < kNum3DOscs) current3dCount++;
            else if (t == 2 && currentVACount < kNumVAOscs) currentVACount++;
            else if (t == 3 && currentWTCount < kNumWTOscs) currentWTCount++;
            else if (t == 4 && currentKSCount < kNumKSOscs) currentKSCount++;
        }
    }

    // ── 3D Oscillator ensemble ────────────────────────────────────────────────
    const auto& c3d = pc.osc3d;
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
    p3d.drive     = juce::jlimit(0.25f, 4.0f,  pF(c3d.drive, 1.f)     + modDests.dests[(int)ModDest::Osc3D_Drive]);

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
    float pitchedHz  = portaHz * std::pow(2.f, totalPitch / 12.f);

    // 3D ensemble — symmetric detune per instance
    float baseHz3d    = pitchedHz * std::pow(2.f, pF(c3d.octave));
    float detuneSemis = pF(pc.slotCfg.osc3dDetune, 0.f);
    for (int i = 0; i < kNum3DOscs; ++i)
    {
        osc3dArray[(size_t)i].setParams(p3d);
        float offset = 0.f;
        if (current3dCount > 1 && detuneSemis > 0.f)
        {
            float pos = (float)i / (float)(current3dCount - 1) * 2.f - 1.f;
            offset = pos * detuneSemis * 0.5f;
        }
        osc3dArray[(size_t)i].setFrequency(baseHz3d * std::pow(2.f, offset / 12.f));
    }
    envBank.get(ENV_3D).setParams(envParams(c3d.env, 0.01f, 0.2f, 1.f, 0.3f));

    // ── FM Engines ────────────────────────────────────────────────────────────
    const auto& cfm = pc.fm;
    int   fmAlgo = pI(cfm.algorithm);
    float fmHz   = pitchedHz * std::pow(2.f, pF(cfm.octave));
    fm.setAlgorithm(fmAlgo);
    fm.setNoteFrequency(fmHz);
    fm.setVelocity(velocity);
    fm.setMidiNote(midiNote);

    if (currentBlockBIsFM)
    {
        float fmDetuneCents = pF(pc.slotCfg.fmDetune, 0.f);
        float hz2 = fmHz * std::pow(2.f, fmDetuneCents / 1200.f);
        fm2.setAlgorithm(fmAlgo);
        fm2.setNoteFrequency(hz2);
        fm2.setVelocity(velocity);
        fm2.setMidiNote(midiNote);
    }

    ADSREnvelope::Params fmLayerEnv = envParams(cfm.env, 0.01f, 0.2f, 1.f, 0.3f);
    envBank.get(ENV_FM).setParams(fmLayerEnv);

    for (int i = 0; i < 10; ++i)
    {
        const auto& opc = pc.ops[(size_t)i];
        FMOperator::Params op;
        float ratioMod   = i < 5 ? modDests.dests[(int)ModDest::FM_Op0_Ratio + i] : 0.f;
        op.coarse        = pI(opc.coarse, 1);
        op.fine          = pF(opc.fine);
        op.ratioOffset   = ratioMod;
        op.level         = juce::jlimit(0.f, 1.f, pF(opc.level, 1.f) +
                                modDests.dests[(int)ModDest::FM_Op0_Level + i]);
        op.feedback      = pF(opc.feedback);
        op.fixedFreq     = pI(opc.fixed) != 0;
        op.waveShape     = static_cast<FMOperator::Wave>(pI(opc.wave));
        op.velocityDepth = pF(opc.vel);
        op.keyScaleDepth = pF(opc.key);
        fm.getOperator(i).setParams(op);
        if (currentBlockBIsFM)
            fm2.getOperator(i).setParams(op);

        ADSREnvelope::Params ep;
        ep.attackSec  = pF(opc.adsrA, 0.01f);
        ep.decaySec   = pF(opc.adsrD, 0.5f);
        ep.sustain    = pF(opc.adsrS, 0.8f);
        ep.releaseSec = juce::jmax(pF(opc.adsrR, 0.3f), fmLayerEnv.releaseSec);
        fm.getOperator(i).getEnvelope().setParams(ep);
        if (currentBlockBIsFM)
            fm2.getOperator(i).getEnvelope().setParams(ep);
    }

    // ── Wavetable ─────────────────────────────────────────────────────────────
    const auto& cwt = pc.wt;
    float wtHz = pitchedHz * std::pow(2.f, pF(cwt.octave));
    for (auto& wt : wtOscs)
    {
        wt.setFrequency(wtHz);
        wt.setTablePosition(pI(cwt.table), pF(cwt.pos) + modDests.dests[(int)ModDest::WT_Position]);
    }
    envBank.get(ENV_WT).setParams(envParams(cwt.env, 0.01f, 0.2f, 1.f, 0.3f));

    // ── VA Oscillators ────────────────────────────────────────────────────────
    for (int i = 0; i < kNumVAOscs; ++i)
    {
        const auto& cv = pc.va[(size_t)i];
        float vaHz = pitchedHz * std::pow(2.f, pF(cv.octave));
        vaOscs[(size_t)i].setFrequency(vaHz);

        VAOscillator::Params vap;
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
    const auto& cks = pc.ks;
    cachedKSParams.decay      = 0.99f + 0.0099f * juce::jlimit(0.f, 1.f, pF(cks.decay, 0.6f));
    cachedKSParams.brightness = juce::jlimit(0.f, 1.f, pF(cks.brightness, 0.5f));
    cachedKSParams.stiffness  = juce::jlimit(0.f, 0.95f, pF(cks.stiffness, 0.f));
    cachedKSParams.morph      = juce::jlimit(0.f, 1.f, pF(cks.morph, 0.f));
    cachedKSParams.position   = juce::jlimit(0.05f, 0.95f, pF(cks.position, 0.3f));
    float ksHz = pitchedHz * std::pow(2.f, pF(cks.octave, 0.f));
    for (auto& ks : ksOscs) ks.setFrequency(ksHz);
    envBank.get(ENV_KS).setParams(envParams(cks.env, 0.001f, 0.1f, 0.f, 1.0f));

    // ── Filters ───────────────────────────────────────────────────────────────
    const auto& cfa = pc.filterA;
    SingleFilter::Params fpA;
    fpA.mode      = static_cast<FilterMode>(pI(cfa.type));
    fpA.cutoff    = juce::jlimit(20.f, (float)(sampleRate * 0.49),
                                 pF(cfa.cutoff, 20000.f) * std::pow(2.f, modDests.dests[(int)ModDest::FilterA_Cutoff] * 4.f));
    fpA.resonance = juce::jlimit(0.f, 1.f, pF(cfa.resonance) + modDests.dests[(int)ModDest::FilterA_Resonance]);
    fpA.drive     = juce::jlimit(0.f, 1.f, pF(cfa.drive)     + modDests.dests[(int)ModDest::FilterA_Drive]);
    fpA.envAmount = pF(cfa.envAmt);
    fpA.lfoAmount = pF(cfa.lfoAmt);
    auto envA = envParams(cfa.env, 0.05f, 0.3f, 0.f, 0.2f);
    dualFilter.getEnvA().setParams(envA);
    envBank.get(ENV_FILTER_A).setParams(envA);

    const auto& cfb = pc.filterB;
    SingleFilter::Params fpB;
    fpB.mode      = static_cast<FilterMode>(pI(cfb.type));
    fpB.cutoff    = juce::jlimit(20.f, (float)(sampleRate * 0.49),
                                 pF(cfb.cutoff, 20000.f) * std::pow(2.f, modDests.dests[(int)ModDest::FilterB_Cutoff] * 4.f));
    fpB.resonance = juce::jlimit(0.f, 1.f, pF(cfb.resonance) + modDests.dests[(int)ModDest::FilterB_Resonance]);
    fpB.drive     = juce::jlimit(0.f, 1.f, pF(cfb.drive)     + modDests.dests[(int)ModDest::FilterB_Drive]);
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
    modSources.sources[(int)ModSource::LFO1] = lfo.getValue(0);
    modSources.sources[(int)ModSource::LFO2] = lfo.getValue(1);
    modSources.sources[(int)ModSource::LFO3] = lfo.getValue(2);
    modSources.sources[(int)ModSource::LFO4] = lfo.getValue(3);

    modSources.sources[(int)ModSource::Env_Master]  = envBank.get(ENV_MASTER).getCurrentValue();
    modSources.sources[(int)ModSource::Env_3D]      = envBank.get(ENV_3D).getCurrentValue();
    modSources.sources[(int)ModSource::Env_FM]      = envBank.get(ENV_FM).getCurrentValue();
    modSources.sources[(int)ModSource::Env_WT]      = envBank.get(ENV_WT).getCurrentValue();
    modSources.sources[(int)ModSource::Env_VA]      = juce::jmax(
        juce::jmax(envBank.get(ENV_VA).getCurrentValue(),  envBank.get(ENV_VA2).getCurrentValue()),
        juce::jmax(envBank.get(ENV_VA3).getCurrentValue(), envBank.get(ENV_VA4).getCurrentValue()));
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
                              juce::AudioProcessorValueTreeState& apvts)
{
    if (!isActive()) return;
    static constexpr float kSilent = 1.0e-5f;

    if (!pc.built) buildParamCache(apvts);

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
    lfo.processBlock(numSamples);

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
    syncParams();

    // ── Level / enable flags ──────────────────────────────────────────────────
    float osc3dLevel = juce::jlimit(0.f, 1.f, pF(pc.osc3d.level, 0.8f) + modDests.dests[(int)ModDest::Osc3D_Level]);
    float fmLevel    = juce::jlimit(0.f, 1.f, pF(pc.fm.level));
    float wtLevel    = juce::jlimit(0.f, 1.f, pF(pc.wt.level) + modDests.dests[(int)ModDest::WT_Level]);
    float ksLevel    = juce::jlimit(0.f, 1.f, pF(pc.ks.level));

    std::array<float, kNumVAOscs> vaLevels {};
    for (int i = 0; i < kNumVAOscs; ++i)
        vaLevels[(size_t)i] = juce::jlimit(0.f, 1.f,
            pF(pc.va[(size_t)i].level) + modDests.dests[(int)vaLevelDest(i)]);

    float masterVol  = juce::jlimit(0.f, 1.f,
        pF(pc.master.volume, 0.8f) + modDests.dests[(int)ModDest::Master_Volume]);
    float masterPan  = juce::jlimit(-1.f, 1.f,
        pF(pc.master.pan) + modDests.dests[(int)ModDest::Master_Pan]);

    float lfoA = lfo.getValue(0);
    float lfoB = lfo.getValue(1);

    const bool use3D = pB(pc.osc3d.on) && osc3dLevel > kSilent && current3dCount > 0;
    const bool useFM = pB(pc.fm.on)    && fmLevel    > kSilent && (currentBlockAIsFM || currentBlockBIsFM);
    const bool useWT = pB(pc.wt.on)    && wtLevel    > kSilent && currentWTCount > 0;
    const bool useKS = pB(pc.ks.on)    && ksLevel    > kSilent && currentKSCount > 0;

    std::array<bool, kNumVAOscs> useVA {};
    for (int i = 0; i < currentVACount; ++i)
        useVA[(size_t)i] = pB(pc.va[(size_t)i].on) && vaLevels[(size_t)i] > kSilent;

    const float panGainL = masterVol * std::sqrt(0.5f - 0.5f * masterPan);
    const float panGainR = masterVol * std::sqrt(0.5f + 0.5f * masterPan);

    const float osc3dNorm = current3dCount > 0
        ? 1.f / std::sqrt((float)current3dCount) : 1.f;
    const bool  dualFM     = currentBlockAIsFM && currentBlockBIsFM;
    const float fmScale    = dualFM ? 0.5f : 1.f;
    const float wtNorm     = currentWTCount > 0
        ? 1.f / std::sqrt((float)currentWTCount) : 1.f;
    const float ksNorm     = currentKSCount > 0
        ? 1.f / (float)currentKSCount : 1.f;

    auto* leftCh  = buffer.getWritePointer(0, startSample);
    auto* rightCh = buffer.getWritePointer(1, startSample);

    ++age;

    for (int s = 0; s < numSamples; ++s)
    {
        float masterEnv = envBank.processAndGet(ENV_MASTER);

        if (masterEnv < kSilent)
        {
            // Drain envelopes while tailing off
            envBank.processAndGet(ENV_3D);
            envBank.processAndGet(ENV_FM);
            envBank.processAndGet(ENV_WT);
            envBank.processAndGet(ENV_KS);
            for (int i = 0; i < kNumVAOscs; ++i)
                envBank.processAndGet(vaEnvIndex(i));
            envBank.processAndGet(ENV_FILTER_A);
            envBank.processAndGet(ENV_FILTER_B);
            fm.advanceEnvelopes();
            if (dualFM) fm2.advanceEnvelopes();
            if (!envBank.anyActive()) { midiNote = -1; break; }
            continue;
        }

        float env3D = envBank.processAndGet(ENV_3D);
        float envFM = envBank.processAndGet(ENV_FM);
        float envWT = envBank.processAndGet(ENV_WT);
        float envKS = envBank.processAndGet(ENV_KS);
        float envVA[kNumVAOscs];
        for (int i = 0; i < kNumVAOscs; ++i)
            envVA[i] = envBank.processAndGet(vaEnvIndex(i));
        envBank.processAndGet(ENV_FILTER_A);
        envBank.processAndGet(ENV_FILTER_B);

        float monoSig = 0.f;

        // 3D ensemble
        if (use3D && env3D > kSilent)
        {
            float sum = 0.f;
            for (int i = 0; i < current3dCount; ++i)
                sum += osc3dArray[(size_t)i].processSample();
            monoSig += sum * osc3dNorm * env3D * osc3dLevel;
        }

        // FM engines
        if (useFM && envFM > kSilent)
        {
            float fmSig = 0.f;
            if (currentBlockAIsFM) fmSig += fm.processSample();
            if (currentBlockBIsFM) fmSig += fm2.processSample();
            monoSig += fmSig * fmScale * envFM * fmLevel;
        }
        else if (useFM)
        {
            fm.advanceEnvelopes();
            if (dualFM) fm2.advanceEnvelopes();
        }

        // KS pool
        for (int i = 0; i < currentKSCount; ++i)
        {
            if (useKS && (ksOscs[(size_t)i].isActive() || envKS > kSilent))
                monoSig += ksOscs[(size_t)i].processSample(cachedKSParams) * envKS * ksLevel * ksNorm;
        }

        float sigL = monoSig;
        float sigR = monoSig;

        // WT pool (stereo)
        if (useWT && envWT > kSilent)
        {
            for (int i = 0; i < currentWTCount; ++i)
            {
                auto [wtL, wtR] = wtOscs[(size_t)i].processSample();
                sigL += wtL * envWT * wtLevel * wtNorm;
                sigR += wtR * envWT * wtLevel * wtNorm;
            }
        }

        // VA instances (stereo, each independent)
        for (int i = 0; i < currentVACount; ++i)
        {
            if (useVA[(size_t)i] && envVA[i] > kSilent)
            {
                auto [vaL, vaR] = vaOscs[(size_t)i].processSample();
                sigL += vaL * envVA[i] * vaLevels[(size_t)i];
                sigR += vaR * envVA[i] * vaLevels[(size_t)i];
            }
        }

        auto [fL, fR] = dualFilter.processSample(sigL, sigR, lfoA, lfoB);

        leftCh[s]  += fL * masterEnv * panGainL;
        rightCh[s] += fR * masterEnv * panGainR;
    }
}
