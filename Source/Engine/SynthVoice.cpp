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
        juce::String pfx = "op" + juce::String(i) + "_";
        FMOperator::Params op;
        float ratioMod = i < 5 ? modDests.dests[(int)ModDest::FM_Op0_Ratio + i] : 0.f;
        op.coarse      = getI(pfx + "coarse");
        op.fine        = getF(pfx + "fine");
        op.ratioOffset = ratioMod;
        op.level       = juce::jlimit(0.f, 1.f, getF(pfx + "level") +
                             modDests.dests[(int)ModDest::FM_Op0_Level + i]);
        op.feedback    = getF(pfx + "feedback");
        op.fixedFreq   = getI(pfx + "fixed") != 0;
        op.waveShape   = static_cast<FMOperator::Wave>(getI(pfx + "wave"));
        op.velocityDepth = getF(pfx + "vel");
        op.keyScaleDepth = getF(pfx + "key");
        fm.getOperator(i).setParams(op);

        ADSREnvelope::Params ep;
        ep.attackSec  = getF(pfx + "adsr_a");
        ep.decaySec   = getF(pfx + "adsr_d");
        ep.sustain    = getF(pfx + "adsr_s");
        ep.releaseSec = juce::jmax(getF(pfx + "adsr_r"), fmLayerEnv.releaseSec);
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

    auto getF = [&](const juce::String& id) -> float {
        auto* p = apvts.getRawParameterValue(id);
        return p ? p->load() : 0.f;
    };
    auto getI = [&](const juce::String& id) -> int {
        auto* p = apvts.getRawParameterValue(id);
        return p ? (int)p->load() : 0;
    };

    // Update LFO params from APVTS once per block
    for (int i = 0; i < LFOEngine::kNumLFOs; ++i)
    {
        juce::String p = "lfo" + juce::String(i + 1) + "_";
        LFOEngine::LFOParams lp;
        lp.shape   = static_cast<LFOEngine::Shape>(getI(p + "shape"));
        lp.rate    = juce::jlimit(0.01f, 100.f,
                                  getF(p + "rate") + modDests.dests[(int)ModDest::LFO1_Rate + i]);
        lp.syncIdx = getI(p + "sync");
        lp.amount  = juce::jlimit(0.f, 1.f,
                                  getF(p + "amount") + modDests.dests[(int)ModDest::LFO1_Amount + i]);
        lp.phase   = getF(p + "phase");
        lp.retrig  = getI(p + "retrig") != 0;
        lfo.setParams(i, lp);
    }
    lfo.processBlock(numSamples);

    // Update mod matrix from APVTS
    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        juce::String p = "mod" + juce::String(i) + "_";
        ModSlot slot;
        slot.source  = static_cast<ModSource>(getI(p + "source"));
        slot.dest    = static_cast<ModDest>(getI(p + "dest"));
        slot.amount  = getF(p + "amount");
        slot.bipolar = getI(p + "bipolar") != 0;
        modMatrix.setSlot(i, slot);
    }

    // Macros
    for (int i = 1; i <= 8; ++i)
        modSources.sources[(int)ModSource::Macro1 + i - 1] = getF("macro" + juce::String(i));

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

    const bool use3D = osc3dLevel > kSilent;
    const bool useFM = fmLevel > kSilent;
    const bool useWT = wtLevel > kSilent;
    const bool useVA = vaLevel > kSilent;
    const float panGainL = masterVol * std::sqrt(0.5f - 0.5f * masterPan);
    const float panGainR = masterVol * std::sqrt(0.5f + 0.5f * masterPan);

    auto* leftCh  = buffer.getWritePointer(0, startSample);
    auto* rightCh = buffer.getWritePointer(1, startSample);

    ++age;

    for (int s = 0; s < numSamples; ++s)
    {
        float masterEnv = envBank.processAndGet(ENV_MASTER);
        float env3D     = envBank.processAndGet(ENV_3D);
        float envFM     = envBank.processAndGet(ENV_FM);
        float envWT     = envBank.processAndGet(ENV_WT);
        float envVA     = envBank.processAndGet(ENV_VA);
        envBank.processAndGet(ENV_FILTER_A);
        envBank.processAndGet(ENV_FILTER_B);

        // Mix oscillators
        float monoSig = 0.f;
        if (use3D && env3D > kSilent)
            monoSig += osc3d.processSample() * env3D * osc3dLevel;

        if (useFM && envFM > kSilent)
        {
            float fmSig = fm.processSample();
            monoSig += fmSig * envFM * fmLevel;
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
