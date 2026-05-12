#include "SynthVoice.h"

float SynthVoice::midiToHz(int note, float pitchBendSemitones)
{
    return 440.f * std::pow(2.f, ((float)(note - 69) + pitchBendSemitones) / 12.f);
}

void SynthVoice::prepare(double sr)
{
    sampleRate = sr;
    osc3d.prepare(sr);
    fm.prepare(sr);
    wtOsc.prepare(sr);
    vaOsc.prepare(sr);
    envBank.prepare(sr);
    lfo.prepare(sr);
    dualFilter.prepare(sr);
}

void SynthVoice::reset()
{
    midiNote = -1;
    age      = 0;
    osc3d.reset();
    fm.reset();
    wtOsc.reset();
    vaOsc.reset();
    envBank.reset();
    lfo.reset();
    dualFilter.reset();
}

void SynthVoice::noteOn(int note, float vel)
{
    midiNote = note;
    velocity = juce::jlimit(0.f, 1.f, vel);
    age      = 0;

    float hz = midiToHz(note);
    osc3d.setFrequency(hz);
    fm.setNoteFrequency(hz);
    wtOsc.setFrequency(hz);
    vaOsc.setFrequency(hz);

    envBank.noteOn();
    fm.noteOn();
    lfo.noteOn();
    dualFilter.noteOn();
}

void SynthVoice::noteOff()
{
    envBank.noteOff();
    fm.noteOff();
    dualFilter.noteOff();
}

void SynthVoice::setHostBPM(double bpm)
{
    lfo.setHostBPM(bpm);
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

//──────────────────────────────────────────────────────────────────────────────
void SynthVoice::buildParamCache(juce::AudioProcessorValueTreeState& apvts)
{
    auto get = [&](const juce::String& id) { return apvts.getRawParameterValue(id); };

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
        l.shape  = get(pfx + "shape");
        l.rate   = get(pfx + "rate");
        l.sync   = get(pfx + "sync");
        l.amount = get(pfx + "amount");
        l.phase  = get(pfx + "phase");
        l.retrig = get(pfx + "retrig");
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

    pc.en3D = get("osc_3d_on");
    pc.enFM = get("osc_fm_on");
    pc.enWT = get("osc_wt_on");
    pc.enVA = get("osc_va_on");

    pc.built = true;
}

//──────────────────────────────────────────────────────────────────────────────
void SynthVoice::syncParams(juce::AudioProcessorValueTreeState& apvts)
{
    auto getF = [&](const juce::String& id) -> float {
        auto* p = apvts.getRawParameterValue(id);
        return p ? p->load() : 0.f;
    };
    auto getI = [&](const juce::String& id) -> int {
        auto* p = apvts.getRawParameterValue(id);
        return p ? (int)p->load() : 0;
    };

    // ── 3D Oscillator ─────────────────────────────────────────────────────
    Osc3D::Params p3d;
    p3d.field    = static_cast<Osc3D::FieldType>(getI("osc_3d_field"));
    p3d.path     = static_cast<Osc3D::PathType>(getI("osc_3d_path"));
    p3d.radius   = juce::jlimit(0.1f, 4.0f, getF("osc_3d_radius") + modDests.dests[(int)ModDest::Osc3D_Radius]);
    p3d.depth    = juce::jlimit(0.0f, 4.0f, getF("osc_3d_depth") + modDests.dests[(int)ModDest::Osc3D_Depth]);
    p3d.warpAmt  = juce::jlimit(0.0f, 2.0f, getF("osc_3d_warp") + modDests.dests[(int)ModDest::Osc3D_Warp]);
    p3d.rotation = getF("osc_3d_rotation");
    p3d.chaos    = juce::jlimit(0.f, 1.f, getF("osc_3d_chaos") + modDests.dests[(int)ModDest::Osc3D_Chaos]);
    p3d.morphT   = juce::jlimit(0.f, 1.f, getF("osc_3d_morph") + modDests.dests[(int)ModDest::Osc3D_Morph]);
    p3d.fieldFreq = juce::jlimit(0.25f, 16.0f, getF("osc_3d_field_freq") + modDests.dests[(int)ModDest::Osc3D_FieldFreq]);
    p3d.zRatio    = juce::jlimit(0.25f, 4.0f, getF("osc_3d_z_ratio") + modDests.dests[(int)ModDest::Osc3D_ZRatio]);
    p3d.foldAmt   = juce::jlimit(0.f, 1.f, getF("osc_3d_fold") + modDests.dests[(int)ModDest::Osc3D_Fold]);
    p3d.drive     = juce::jlimit(0.25f, 4.0f, getF("osc_3d_drive") + modDests.dests[(int)ModDest::Osc3D_Drive]);
    osc3d.setParams(p3d);

    // Apply global pitch offsets independently: octave, fine tune, then modulation.
    float pitchMod      = modDests.dests[(int)ModDest::OscPitch_All];
    float tuneSemi      = getF("master_tune") / 100.f;  // cents → semitones
    float octaveSemi    = (float)getI("master_octave") * 12.f;
    float totalPitch    = pitchMod * 12.f + tuneSemi + octaveSemi;

    float hz = midiToHz(midiNote, totalPitch) * std::pow(2.f, getF("osc_3d_octave"));
    osc3d.setFrequency(hz);

    // Per-oscillator ADSR
    {
        ADSREnvelope::Params ep;
        ep.attackSec  = getF("osc_3d_adsr_a");
        ep.decaySec   = getF("osc_3d_adsr_d");
        ep.sustain    = getF("osc_3d_adsr_s");
        ep.releaseSec = getF("osc_3d_adsr_r");
        envBank.get(ENV_3D).setParams(ep);
    }

    // ── FM Engine ─────────────────────────────────────────────────────────
    fm.setAlgorithm(getI("osc_fm_algorithm"));
    hz = midiToHz(midiNote, totalPitch) * std::pow(2.f, getF("osc_fm_octave"));
    fm.setNoteFrequency(hz);
    fm.setVelocity(velocity);
    fm.setMidiNote(midiNote);

    ADSREnvelope::Params fmLayerEnv;
    fmLayerEnv.attackSec  = getF("osc_fm_adsr_a");
    fmLayerEnv.decaySec   = getF("osc_fm_adsr_d");
    fmLayerEnv.sustain    = getF("osc_fm_adsr_s");
    fmLayerEnv.releaseSec = getF("osc_fm_adsr_r");
    envBank.get(ENV_FM).setParams(fmLayerEnv);

    for (int i = 0; i < 10; ++i)
    {
        const auto& opc = pc.ops[(size_t)i];
        auto pF = [](std::atomic<float>* p, float def = 0.f) { return p ? p->load() : def; };
        auto pI = [](std::atomic<float>* p, int   def = 0  ) { return p ? (int)p->load() : def; };

        FMOperator::Params op;
        float ratioMod    = i < 5 ? modDests.dests[(int)ModDest::FM_Op0_Ratio + i] : 0.f;
        op.coarse         = pI(opc.coarse, 1);
        op.fine           = pF(opc.fine);
        op.ratioOffset    = ratioMod;
        op.level          = juce::jlimit(0.f, 1.f, pF(opc.level, 1.f) +
                                modDests.dests[(int)ModDest::FM_Op0_Level + i]);
        op.feedback       = pF(opc.feedback);
        op.fixedFreq      = pI(opc.fixed) != 0;
        op.waveShape      = static_cast<FMOperator::Wave>(pI(opc.wave));
        op.velocityDepth  = pF(opc.vel);
        op.keyScaleDepth  = pF(opc.key);
        fm.getOperator(i).setParams(op);

        ADSREnvelope::Params ep;
        ep.attackSec  = pF(opc.adsrA, 0.01f);
        ep.decaySec   = pF(opc.adsrD, 0.5f);
        ep.sustain    = pF(opc.adsrS, 0.8f);
        ep.releaseSec = juce::jmax(pF(opc.adsrR, 0.3f), fmLayerEnv.releaseSec);
        fm.getOperator(i).getEnvelope().setParams(ep);
    }

    // ── Wavetable ─────────────────────────────────────────────────────────
    float wtHz = midiToHz(midiNote, totalPitch) * std::pow(2.f, getF("osc_wt_octave"));
    wtOsc.setFrequency(wtHz);
    wtOsc.setTablePosition(getI("osc_wt_table"), getF("osc_wt_pos") + modDests.dests[(int)ModDest::WT_Position]);

    {
        ADSREnvelope::Params ep;
        ep.attackSec  = getF("osc_wt_adsr_a");
        ep.decaySec   = getF("osc_wt_adsr_d");
        ep.sustain    = getF("osc_wt_adsr_s");
        ep.releaseSec = getF("osc_wt_adsr_r");
        envBank.get(ENV_WT).setParams(ep);
    }

    // ── VA Oscillator ─────────────────────────────────────────────────────
    float vaHz = midiToHz(midiNote, totalPitch) * std::pow(2.f, getF("osc_va_octave"));
    vaOsc.setFrequency(vaHz);
    VAOscillator::Params vap;
    vap.shape      = static_cast<VAOscillator::Shape>(getI("osc_va_shape"));
    vap.pulseWidth = juce::jlimit(0.05f, 0.95f, getF("osc_va_pw") + modDests.dests[(int)ModDest::VA_PW]);
    vap.detune     = getF("osc_va_detune") + modDests.dests[(int)ModDest::VA_Detune];
    vap.unisonCount = getI("osc_va_unison");
    vap.spread     = getF("osc_va_spread") + modDests.dests[(int)ModDest::VA_Spread];
    vaOsc.setParams(vap);

    {
        ADSREnvelope::Params ep;
        ep.attackSec  = getF("osc_va_adsr_a");
        ep.decaySec   = getF("osc_va_adsr_d");
        ep.sustain    = getF("osc_va_adsr_s");
        ep.releaseSec = getF("osc_va_adsr_r");
        envBank.get(ENV_VA).setParams(ep);
    }

    // ── Filters ───────────────────────────────────────────────────────────
    SingleFilter::Params fpA;
    fpA.mode      = static_cast<FilterMode>(getI("filter_a_type"));
    fpA.cutoff    = juce::jlimit(20.f, (float)(sampleRate * 0.49),
                                 getF("filter_a_cutoff") * std::pow(2.f, modDests.dests[(int)ModDest::FilterA_Cutoff] * 4.f));
    fpA.resonance = juce::jlimit(0.f, 1.f, getF("filter_a_resonance") + modDests.dests[(int)ModDest::FilterA_Resonance]);
    fpA.drive     = juce::jlimit(0.f, 1.f, getF("filter_a_drive") + modDests.dests[(int)ModDest::FilterA_Drive]);
    fpA.envAmount = getF("filter_a_env_amt");
    fpA.lfoAmount = getF("filter_a_lfo_amt");

    {
        ADSREnvelope::Params ep;
        ep.attackSec  = getF("filter_a_adsr_a");
        ep.decaySec   = getF("filter_a_adsr_d");
        ep.sustain    = getF("filter_a_adsr_s");
        ep.releaseSec = getF("filter_a_adsr_r");
        dualFilter.getEnvA().setParams(ep);
        envBank.get(ENV_FILTER_A).setParams(ep);
    }

    SingleFilter::Params fpB;
    fpB.mode      = static_cast<FilterMode>(getI("filter_b_type"));
    fpB.cutoff    = juce::jlimit(20.f, (float)(sampleRate * 0.49),
                                 getF("filter_b_cutoff") * std::pow(2.f, modDests.dests[(int)ModDest::FilterB_Cutoff] * 4.f));
    fpB.resonance = juce::jlimit(0.f, 1.f, getF("filter_b_resonance") + modDests.dests[(int)ModDest::FilterB_Resonance]);
    fpB.drive     = juce::jlimit(0.f, 1.f, getF("filter_b_drive") + modDests.dests[(int)ModDest::FilterB_Drive]);
    fpB.envAmount = getF("filter_b_env_amt");
    fpB.lfoAmount = getF("filter_b_lfo_amt");

    {
        ADSREnvelope::Params ep;
        ep.attackSec  = getF("filter_b_adsr_a");
        ep.decaySec   = getF("filter_b_adsr_d");
        ep.sustain    = getF("filter_b_adsr_s");
        ep.releaseSec = getF("filter_b_adsr_r");
        dualFilter.getEnvB().setParams(ep);
        envBank.get(ENV_FILTER_B).setParams(ep);
    }

    dualFilter.setFilterA(fpA);
    dualFilter.setFilterB(fpB);
    dualFilter.setRouting(static_cast<FilterRouting>(getI("filter_routing")));

    // ── Master ADSR ───────────────────────────────────────────────────────
    {
        ADSREnvelope::Params ep;
        ep.attackSec  = getF("master_adsr_a");
        ep.decaySec   = getF("master_adsr_d");
        ep.sustain    = getF("master_adsr_s");
        ep.releaseSec = getF("master_adsr_r");
        envBank.get(ENV_MASTER).setParams(ep);
    }
}

//──────────────────────────────────────────────────────────────────────────────
void SynthVoice::buildModSources()
{
    // Copy LFO values
    modSources.sources[(int)ModSource::LFO1] = lfo.getValue(0);
    modSources.sources[(int)ModSource::LFO2] = lfo.getValue(1);
    modSources.sources[(int)ModSource::LFO3] = lfo.getValue(2);
    modSources.sources[(int)ModSource::LFO4] = lfo.getValue(3);

    // Envelope values (0..1)
    modSources.sources[(int)ModSource::Env_Master]  = envBank.get(ENV_MASTER).getCurrentValue();
    modSources.sources[(int)ModSource::Env_3D]      = envBank.get(ENV_3D).getCurrentValue();
    modSources.sources[(int)ModSource::Env_FM]      = envBank.get(ENV_FM).getCurrentValue();
    modSources.sources[(int)ModSource::Env_WT]      = envBank.get(ENV_WT).getCurrentValue();
    modSources.sources[(int)ModSource::Env_VA]      = envBank.get(ENV_VA).getCurrentValue();
    modSources.sources[(int)ModSource::Env_FilterA] = envBank.get(ENV_FILTER_A).getCurrentValue();
    modSources.sources[(int)ModSource::Env_FilterB] = envBank.get(ENV_FILTER_B).getCurrentValue();

    modSources.sources[(int)ModSource::Velocity]     = velocity;
    modSources.sources[(int)ModSource::Aftertouch]   = midiCCs[(int)ModSource::Aftertouch];
    modSources.sources[(int)ModSource::ModWheel]     = midiCCs[(int)ModSource::ModWheel];
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

    // Build param pointer cache once on first block (avoids per-block string alloc).
    if (!pc.built) buildParamCache(apvts);

    auto getF = [&](const juce::String& id) -> float {
        auto* p = apvts.getRawParameterValue(id);
        return p ? p->load() : 0.f;
    };

    // Update LFO params using cached pointers — no string construction
    for (int i = 0; i < LFOEngine::kNumLFOs; ++i)
    {
        const auto& lc = pc.lfos[(size_t)i];
        auto pF = [](std::atomic<float>* p, float def = 0.f) { return p ? p->load() : def; };
        auto pI = [](std::atomic<float>* p, int   def = 0  ) { return p ? (int)p->load() : def; };

        LFOEngine::LFOParams lp;
        lp.shape   = static_cast<LFOEngine::Shape>(pI(lc.shape));
        lp.rate    = juce::jlimit(0.01f, 100.f,
                                  pF(lc.rate, 1.f) + modDests.dests[(int)ModDest::LFO1_Rate + i]);
        lp.syncIdx = pI(lc.sync);
        lp.amount  = juce::jlimit(0.f, 1.f,
                                  pF(lc.amount, 0.5f) + modDests.dests[(int)ModDest::LFO1_Amount + i]);
        lp.phase   = pF(lc.phase);
        lp.retrig  = pI(lc.retrig) != 0;
        lfo.setParams(i, lp);
    }
    lfo.processBlock(numSamples);

    // Update mod matrix using cached pointers — no string construction
    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const auto& mc = pc.mods[(size_t)i];
        ModSlot slot;
        slot.source  = static_cast<ModSource>(mc.source  ? (int)mc.source->load()  : 0);
        slot.dest    = static_cast<ModDest>  (mc.dest    ? (int)mc.dest->load()    : 0);
        slot.amount  = mc.amount  ? mc.amount->load()  : 0.f;
        slot.bipolar = mc.bipolar ? mc.bipolar->load() > 0.5f : true;
        modMatrix.setSlot(i, slot);
    }

    // Macros — cached pointers
    for (int i = 0; i < 8; ++i)
        modSources.sources[(int)ModSource::Macro1 + i] =
            pc.macros[(size_t)i] ? pc.macros[(size_t)i]->load() : 0.f;

    buildModSources();
    modMatrix.process(modSources, modDests);

    syncParams(apvts);

    float osc3dLevel = juce::jlimit(0.f, 1.f, getF("osc_3d_level") + modDests.dests[(int)ModDest::Osc3D_Level]);
    float fmLevel    = juce::jlimit(0.f, 1.f, getF("osc_fm_level"));
    float wtLevel    = juce::jlimit(0.f, 1.f, getF("osc_wt_level") + modDests.dests[(int)ModDest::WT_Level]);
    float vaLevel    = juce::jlimit(0.f, 1.f, getF("osc_va_level") + modDests.dests[(int)ModDest::VA_Level]);

    float masterVol  = getF("master_volume");
    float masterPan  = juce::jlimit(-1.f, 1.f, getF("master_pan") + modDests.dests[(int)ModDest::Master_Pan]);
    masterVol = juce::jlimit(0.f, 1.f, masterVol + modDests.dests[(int)ModDest::Master_Volume]);

    float lfoA = lfo.getValue(0);
    float lfoB = lfo.getValue(1);

    auto loadBool = [](std::atomic<float>* p) { return p == nullptr || p->load() > 0.5f; };
    const bool use3D = loadBool(pc.en3D) && osc3dLevel > kSilent;
    const bool useFM = loadBool(pc.enFM) && fmLevel    > kSilent;
    const bool useWT = loadBool(pc.enWT) && wtLevel    > kSilent;
    const bool useVA = loadBool(pc.enVA) && vaLevel    > kSilent;
    const float panGainL = masterVol * std::sqrt(0.5f - 0.5f * masterPan);
    const float panGainR = masterVol * std::sqrt(0.5f + 0.5f * masterPan);

    auto* leftCh  = buffer.getWritePointer(0, startSample);
    auto* rightCh = buffer.getWritePointer(1, startSample);

    ++age;

    for (int s = 0; s < numSamples; ++s)
    {
        float masterEnv = envBank.processAndGet(ENV_MASTER);

        // Once the master envelope is below audibility, drain remaining
        // envelopes cheaply and stop rendering DSP for this voice.
        // This is the primary fix for CPU spikes during long release tails:
        // previously all oscillators and the filter kept running until the
        // very last sample of the release, at full cost per voice.
        if (masterEnv < kSilent)
        {
            envBank.processAndGet(ENV_3D);
            envBank.processAndGet(ENV_FM);
            envBank.processAndGet(ENV_WT);
            envBank.processAndGet(ENV_VA);
            envBank.processAndGet(ENV_FILTER_A);
            envBank.processAndGet(ENV_FILTER_B);
            fm.advanceEnvelopes();
            if (!envBank.anyActive())
            {
                midiNote = -1;  // mark voice free immediately
                break;
            }
            continue;
        }

        float env3D = envBank.processAndGet(ENV_3D);
        float envFM = envBank.processAndGet(ENV_FM);
        float envWT = envBank.processAndGet(ENV_WT);
        float envVA = envBank.processAndGet(ENV_VA);
        envBank.processAndGet(ENV_FILTER_A);
        envBank.processAndGet(ENV_FILTER_B);

        // Mix oscillators
        float monoSig = 0.f;
        if (use3D && env3D > kSilent)
            monoSig += osc3d.processSample() * env3D * osc3dLevel;

        if (useFM && envFM > kSilent)
        {
            monoSig += fm.processSample() * envFM * fmLevel;
        }
        else if (useFM)
        {
            fm.advanceEnvelopes();
        }

        float sigL = monoSig;
        float sigR = monoSig;

        if (useWT && envWT > kSilent)
        {
            auto [wtL, wtR] = wtOsc.processSample();
            sigL += wtL * envWT * wtLevel;
            sigR += wtR * envWT * wtLevel;
        }

        if (useVA && envVA > kSilent)
        {
            auto [vaL, vaR] = vaOsc.processSample();
            sigL += vaL * envVA * vaLevel;
            sigR += vaR * envVA * vaLevel;
        }

        // Dual filter
        auto [fL, fR] = dualFilter.processSample(sigL, sigR, lfoA, lfoB);

        // Master ADSR, volume, pan
        leftCh[s]  += fL * masterEnv * panGainL;
        rightCh[s] += fR * masterEnv * panGainR;
    }
}
