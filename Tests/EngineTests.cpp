#include <catch2/catch_all.hpp>
#include "../Source/Engine/Envelopes/ADSREnvelope.h"
#include "../Source/Engine/Oscillators/Osc3D.h"
#include "../Source/Engine/Oscillators/VAOscillator.h"
#include "../Source/Engine/Oscillators/WavetableOsc.h"
#include "../Source/Engine/Oscillators/GranularOsc.h"
#include "../Source/Engine/Oscillators/FMOperator.h"
#include "../Source/Engine/Oscillators/FMAlgorithm.h"
#include "../Source/Engine/Filter/DualFilter.h"
#include "../Source/Engine/Modulation/ModMatrix.h"
#include "../Source/Engine/LFO/LFOEngine.h"
#include "../Source/Engine/Arpeggiator/Arpeggiator.h"
#include <vector>

using Catch::Approx;

static std::vector<int> collectArpNoteOns(Arpeggiator& arp, int numSamples)
{
    juce::MidiBuffer midi;
    arp.processBlock(midi, 0, numSamples);

    std::vector<int> notes;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            notes.push_back(msg.getNoteNumber());
    }

    return notes;
}

//──────────────────────────────────────────────────────────────────────────────
TEST_CASE("ADSREnvelope basic lifecycle", "[envelope]")
{
    ADSREnvelope env;
    env.prepare(48000.0);

    SECTION("idle before noteOn")
    {
        REQUIRE(!env.isActive());
        REQUIRE(env.processSample() == Approx(0.f));
    }

    SECTION("attacks from zero after noteOn")
    {
        env.noteOn();
        REQUIRE(env.isActive());
        float v0 = env.processSample();
        float v1 = env.processSample();
        REQUIRE(v1 > v0);
        REQUIRE(v0 >= 0.f);
    }

    SECTION("reaches sustain level after attack+decay")
    {
        ADSREnvelope::Params p;
        p.attackSec  = 0.001f;
        p.decaySec   = 0.001f;
        p.sustain    = 0.5f;
        p.releaseSec = 0.1f;
        env.setParams(p);
        env.noteOn();

        float v = 0.f;
        for (int i = 0; i < 500; ++i) v = env.processSample();
        REQUIRE(v == Approx(0.5f).margin(0.05f));
    }

    SECTION("returns to idle after release")
    {
        ADSREnvelope::Params p;
        p.attackSec  = 0.001f;
        p.decaySec   = 0.001f;
        p.sustain    = 0.5f;
        p.releaseSec = 0.001f;
        env.setParams(p);
        env.noteOn();
        for (int i = 0; i < 200; ++i) env.processSample();
        env.noteOff();
        for (int i = 0; i < 500; ++i) env.processSample();
        REQUIRE(!env.isActive());
    }

    SECTION("long release stays active past the previous 10 second ceiling")
    {
        env.prepare(1000.0);
        ADSREnvelope::Params p;
        p.attackSec  = 0.001f;
        p.decaySec   = 0.001f;
        p.sustain    = 1.0f;
        p.releaseSec = 60.0f;
        env.setParams(p);
        env.noteOn();
        for (int i = 0; i < 10; ++i) env.processSample();

        env.noteOff();
        float v = 0.f;
        for (int i = 0; i < 10000; ++i) v = env.processSample();

        REQUIRE(env.isActive());
        REQUIRE(v > 0.15f);
    }

    SECTION("stacked long layer and master releases keep a pad tail audible")
    {
        ADSREnvelope layer;
        ADSREnvelope master;
        layer.prepare(1000.0);
        master.prepare(1000.0);

        ADSREnvelope::Params p;
        p.attackSec  = 0.001f;
        p.decaySec   = 0.001f;
        p.sustain    = 1.0f;
        p.releaseSec = 60.0f;
        layer.setParams(p);
        master.setParams(p);

        layer.noteOn();
        master.noteOn();
        for (int i = 0; i < 10; ++i)
        {
            layer.processSample();
            master.processSample();
        }

        layer.noteOff();
        master.noteOff();
        float amp = 0.f;
        for (int i = 0; i < 10000; ++i)
            amp = layer.processSample() * master.processSample();

        REQUIRE(layer.isActive());
        REQUIRE(master.isActive());
        REQUIRE(amp > 0.03f);
    }

    SECTION("zero sustain decays to silence")
    {
        ADSREnvelope::Params p;
        p.attackSec  = 0.001f;
        p.decaySec   = 0.001f;
        p.sustain    = 0.0f;
        p.releaseSec = 0.1f;
        env.setParams(p);
        env.noteOn();
        float v = 0.f;
        for (int i = 0; i < 500; ++i) v = env.processSample();
        REQUIRE(v == Approx(0.f).margin(0.05f));
    }

    SECTION("reset returns to idle immediately")
    {
        env.noteOn();
        for (int i = 0; i < 10; ++i) env.processSample();
        env.reset();
        REQUIRE(!env.isActive());
        REQUIRE(env.processSample() == Approx(0.f));
    }

    SECTION("noteOff during attack triggers release")
    {
        ADSREnvelope::Params p;
        p.attackSec  = 1.0f;    // slow attack so we're mid-attack at noteOff
        p.decaySec   = 0.001f;
        p.sustain    = 0.8f;
        p.releaseSec = 0.001f;  // fast release
        env.setParams(p);
        env.noteOn();
        for (int i = 0; i < 24; ++i) env.processSample();
        REQUIRE(env.isActive());
        env.noteOff();
        for (int i = 0; i < 1000; ++i) env.processSample();
        REQUIRE(!env.isActive());
    }
}

//──────────────────────────────────────────────────────────────────────────────
TEST_CASE("Osc3D outputs bounded signal", "[osc3d]")
{
    Osc3D osc;
    osc.prepare(48000.0);
    osc.setFrequency(440.f);

    SECTION("all fields produce output in [-1, 1]")
    {
        for (int f = 0; f < (int)Osc3D::FieldType::NumFields; ++f)
        {
            Osc3D::Params p;
            p.field = static_cast<Osc3D::FieldType>(f);
            osc.setParams(p);
            osc.reset();
            for (int i = 0; i < 4800; ++i)
            {
                float s = osc.processSample();
                REQUIRE(s >= -1.01f);
                REQUIRE(s <=  1.01f);
            }
        }
    }

    SECTION("all path types produce output in [-1, 1]")
    {
        for (int pt = 0; pt < (int)Osc3D::PathType::NumPaths; ++pt)
        {
            Osc3D::Params p;
            p.path = static_cast<Osc3D::PathType>(pt);
            osc.setParams(p);
            osc.reset();
            for (int i = 0; i < 2400; ++i)
            {
                float s = osc.processSample();
                REQUIRE(s >= -1.01f);
                REQUIRE(s <=  1.01f);
            }
        }
    }

    SECTION("math shaping controls remain bounded")
    {
        Osc3D::Params p;
        p.fieldFreq = 12.f;
        p.zRatio    = 3.5f;
        p.foldAmt   = 1.f;
        p.drive     = 4.f;
        p.warpAmt   = 1.5f;
        p.chaos     = 0.75f;
        osc.setParams(p);
        osc.reset();

        for (int i = 0; i < 4800; ++i)
        {
            float s = osc.processSample();
            REQUIRE(std::isfinite(s));
            REQUIRE(s >= -1.01f);
            REQUIRE(s <=  1.01f);
        }
    }
}

//──────────────────────────────────────────────────────────────────────────────
TEST_CASE("VAOscillator stereo output bounds", "[vaosc]")
{
    VAOscillator va;
    va.prepare(48000.0);
    va.setFrequency(440.f);

    for (int shape = 0; shape < 6; ++shape)
    {
        VAOscillator::Params p;
        p.shape       = static_cast<VAOscillator::Shape>(shape);
        p.unisonCount = 3;
        p.spread      = 0.5f;
        va.setParams(p);
        va.reset();

        for (int i = 0; i < 4800; ++i)
        {
            auto [L, R] = va.processSample();
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            // Allow some headroom for unison summing (3 voices)
            REQUIRE(L >= -4.f);
            REQUIRE(L <=  4.f);
            REQUIRE(R >= -4.f);
            REQUIRE(R <=  4.f);
        }
    }

    SECTION("shape morph, ring modulation and wavefolding stay finite")
    {
        VAOscillator::Params p;
        p.shapeMorph  = 2.5f;
        p.pulseWidth  = 0.35f;
        p.ringMod     = 0.75f;
        p.wavefold    = 0.8f;
        p.unisonCount = 4;
        p.spread      = 0.75f;
        va.setParams(p);
        va.reset();

        for (int i = 0; i < 4800; ++i)
        {
            auto [L, R] = va.processSample();
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            REQUIRE(L >= -4.f);
            REQUIRE(L <=  4.f);
            REQUIRE(R >= -4.f);
            REQUIRE(R <=  4.f);
        }
    }
}

//──────────────────────────────────────────────────────────────────────────────
TEST_CASE("LFOEngine shape output ranges", "[lfo]")
{
    LFOEngine lfo;
    lfo.prepare(48000.0);
    lfo.noteOn();

    // Sine, Triangle, Saw, RevSaw, Square all produce -1..1 (scaled by amount=1)
    const LFOEngine::Shape shapes[] = {
        LFOEngine::Shape::Sine,
        LFOEngine::Shape::Triangle,
        LFOEngine::Shape::Saw,
        LFOEngine::Shape::RevSaw,
        LFOEngine::Shape::Square,
    };

    for (auto shape : shapes)
    {
        LFOEngine::LFOParams p;
        p.shape  = shape;
        p.rate   = 5.f;
        p.amount = 1.f;
        lfo.setParams(0, p);

        float mn = 1.f, mx = -1.f;
        for (int block = 0; block < 100; ++block)
        {
            lfo.processBlock(128);
            float v = lfo.getValue(0);
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
        // Should swing both positive and negative and not exceed ±1
        REQUIRE(mx <=  1.01f);
        REQUIRE(mn >= -1.01f);
        REQUIRE(mx > 0.f);   // reaches positive territory
        REQUIRE(mn < 0.f);   // reaches negative territory
    }
}

//──────────────────────────────────────────────────────────────────────────────
TEST_CASE("ModMatrix accumulates correctly", "[modmatrix]")
{
    ModMatrix mm;
    mm.clear();

    ModSlot slot;
    slot.source  = ModSource::LFO1;
    slot.dest    = ModDest::FilterA_Cutoff;
    slot.amount  = 0.5f;
    slot.bipolar = false;
    mm.setSlot(0, slot);

    ModValues src{}, dst{};
    src.sources[(int)ModSource::LFO1] = 0.8f;
    mm.process(src, dst);

    // Unipolar (bipolar=false): dest = srcVal * amount = 0.8 * 0.5 = 0.4
    REQUIRE(dst.dests[(int)ModDest::FilterA_Cutoff] == Approx(0.4f));
}

TEST_CASE("ModMatrix bipolar conversion", "[modmatrix]")
{
    ModMatrix mm;
    mm.clear();

    ModSlot slot;
    slot.source  = ModSource::Velocity;
    slot.dest    = ModDest::FilterA_Cutoff;
    slot.amount  = 0.5f;
    slot.bipolar = true;   // converts 0..1 → -1..1
    mm.setSlot(0, slot);

    ModValues src{}, dst{};
    src.sources[(int)ModSource::Velocity] = 0.8f;
    mm.process(src, dst);

    // Bipolar: srcVal = 0.8*2-1 = 0.6, dest = 0.6 * 0.5 = 0.3
    REQUIRE(dst.dests[(int)ModDest::FilterA_Cutoff] == Approx(0.3f));
}

TEST_CASE("ModMatrix leaves naturally bipolar sources unchanged", "[modmatrix]")
{
    ModMatrix mm;
    mm.clear();

    ModSlot slot;
    slot.source  = ModSource::LFO1;
    slot.dest    = ModDest::FilterA_Cutoff;
    slot.amount  = 0.5f;
    slot.bipolar = true;
    mm.setSlot(0, slot);

    ModValues src{}, dst{};
    src.sources[(int)ModSource::LFO1] = 0.8f;
    mm.process(src, dst);

    REQUIRE(dst.dests[(int)ModDest::FilterA_Cutoff] == Approx(0.4f));
}

TEST_CASE("ModMatrix None slot is skipped", "[modmatrix]")
{
    ModMatrix mm;
    mm.clear();

    ModValues src{}, dst{};
    src.sources[(int)ModSource::LFO1] = 1.0f;
    mm.process(src, dst);

    // All slots are None — all destinations should be zero
    for (const auto& d : dst.dests)
        REQUIRE(d == Approx(0.f));
}

//──────────────────────────────────────────────────────────────────────────────
TEST_CASE("DualFilter LP12 attenuates high freq", "[filter]")
{
    DualFilter df;
    df.prepare(48000.0);

    SingleFilter::Params p;
    p.mode     = FilterMode::LP12;
    p.cutoff   = 500.f;
    p.resonance = 0.f;
    df.setFilterA(p);
    df.setRouting(FilterRouting::AOnly);
    df.noteOn();

    // Generate 10kHz tone and check RMS is attenuated
    float sumSq = 0.f;
    for (int i = 0; i < 4800; ++i)
    {
        float sin10k = std::sin(2.f * 3.14159f * 10000.f * (float)i / 48000.f);
        auto [L, R] = df.processSample(sin10k, sin10k, 0.f, 0.f);
        sumSq += L * L;
    }
    float rms = std::sqrt(sumSq / 4800.f);
    REQUIRE(rms < 0.1f);  // should be heavily attenuated
}

TEST_CASE("DualFilter output is finite", "[filter]")
{
    DualFilter df;
    df.prepare(48000.0);

    // Test all filter modes produce finite output
    for (int mode = 0; mode < static_cast<int>(FilterMode::NumModes); ++mode)
    {
        SingleFilter::Params p;
        p.mode      = static_cast<FilterMode>(mode);
        p.cutoff    = 1000.f;
        p.resonance = 0.5f;
        p.drive     = 0.3f;
        p.morph     = 0.5f;
        p.saturation = 0.6f;
        p.mix       = 1.f;
        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        for (int i = 0; i < 512; ++i)
        {
            float s = std::sin(2.f * 3.14159f * 440.f * (float)i / 48000.f);
            auto [L, R] = df.processSample(s, s, 0.f, 0.f);
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
        }
    }
}

TEST_CASE("DualFilter advanced models stay finite with oversampling", "[filter]")
{
    DualFilter df;
    df.prepare(48000.0);
    df.setRouting(FilterRouting::AOnly);
    df.noteOn();

    const FilterMode modes[] {
        FilterMode::Ladder12,
        FilterMode::Ladder18,
        FilterMode::Ladder24,
        FilterMode::SEMMorph,
        FilterMode::MSDual,
        FilterMode::Formant
    };

    for (auto mode : modes)
    {
        SingleFilter::Params p;
        p.mode = mode;
        p.cutoff = 850.f;
        p.resonance = 0.78f;
        p.drive = 0.65f;
        p.morph = 0.35f;
        p.saturation = 0.7f;
        p.bassComp = 0.5f;
        p.outputGainDb = 3.f;
        p.mix = 1.f;
        p.oversample = 2;
        df.setFilterA(p);

        for (int i = 0; i < 1024; ++i)
        {
            float s = (i == 0 ? 1.f : 0.f) + 0.25f * std::sin(2.f * 3.14159f * 220.f * (float)i / 48000.f);
            auto [L, R] = df.processSample(s, s, 0.1f, -0.2f);
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            REQUIRE(std::abs(L) <= 1.5f);
            REQUIRE(std::abs(R) <= 1.5f);
        }
    }
}

TEST_CASE("DualFilter wet dry mix can bypass filter output", "[filter]")
{
    DualFilter df;
    df.prepare(48000.0);

    SingleFilter::Params p;
    p.mode = FilterMode::Ladder24;
    p.cutoff = 200.f;
    p.resonance = 0.8f;
    p.drive = 0.8f;
    p.mix = 0.f;
    df.setFilterA(p);
    df.setRouting(FilterRouting::AOnly);
    df.noteOn();

    for (int i = 0; i < 1024; ++i)
        (void) df.processSample(0.f, 0.f, 0.f, 0.f);

    for (int i = 0; i < 16; ++i)
    {
        const float s = i == 0 ? 0.75f : 0.f;
        auto [L, R] = df.processSample(s, -s, 0.f, 0.f);
        REQUIRE(L == Approx(s).margin(1.0e-5f));
        REQUIRE(R == Approx(-s).margin(1.0e-5f));
    }
}

TEST_CASE("DualFilter keeps stereo channel state independent", "[filter]")
{
    DualFilter df;
    df.prepare(48000.0);

    SingleFilter::Params p;
    p.mode   = FilterMode::LP12;
    p.cutoff = 1000.f;
    df.setFilterA(p);
    df.setRouting(FilterRouting::AOnly);
    df.noteOn();

    for (int i = 0; i < 64; ++i)
    {
        float leftIn = i == 0 ? 1.f : 0.f;
        auto [L, R] = df.processSample(leftIn, 0.f, 0.f, 0.f);
        (void)L;
        REQUIRE(R == Approx(0.f).margin(1.0e-6f));
    }
}

//──────────────────────────────────────────────────────────────────────────────
TEST_CASE("WavetableOsc table index selects base table", "[wavetable]")
{
    WavetableOsc wt;
    wt.prepare(48000.0);
    wt.setFrequency(440.f);

    std::vector<float> one(WavetableOsc::kTableSize, 1.f);
    std::vector<float> minusOne(WavetableOsc::kTableSize, -1.f);
    int oneIndex = wt.addTable("One", one.data(), (int)one.size());
    int minusOneIndex = wt.addTable("MinusOne", minusOne.data(), (int)minusOne.size());

    wt.setTablePosition(oneIndex, 0.f);
    auto [oneL, oneR] = wt.processSample();
    REQUIRE(oneL == Approx(1.f).margin(0.001f));
    REQUIRE(oneR == Approx(1.f).margin(0.001f));

    wt.reset();
    wt.setTablePosition(minusOneIndex, 0.f);
    auto [minusL, minusR] = wt.processSample();
    REQUIRE(minusL == Approx(-1.f).margin(0.001f));
    REQUIRE(minusR == Approx(-1.f).margin(0.001f));
}

TEST_CASE("WavetableOsc warp modes stay finite and bounded", "[wavetable]")
{
    WavetableOsc wt;
    wt.prepare(48000.0);
    wt.setFrequency(880.f);
    wt.setTablePosition(2, 0.35f);

    for (int mode = 0; mode < (int)WavetableOsc::WarpMode::NumModes; ++mode)
    {
        WavetableOsc::Params p;
        p.warpMode = static_cast<WavetableOsc::WarpMode>(mode);
        p.warpAmount = 0.85f;
        p.unison = 3;
        p.detune = 0.4f;
        p.spread = 0.75f;
        p.positionSpread = 0.2f;
        p.retrigger = true;
        wt.setParams(p);
        wt.reset();

        for (int i = 0; i < 4096; ++i)
        {
            auto [l, r] = wt.processSample();
            REQUIRE(std::isfinite(l));
            REQUIRE(std::isfinite(r));
            REQUIRE(l >= -1.55f);
            REQUIRE(l <=  1.55f);
            REQUIRE(r >= -1.55f);
            REQUIRE(r <=  1.55f);
        }
    }
}

TEST_CASE("WavetableOsc phase offset controls initial phase", "[wavetable]")
{
    WavetableOsc wt;
    wt.prepare(48000.0);
    wt.setFrequency(440.f);
    wt.setTablePosition(0, 0.f);

    WavetableOsc::Params p;
    p.phaseOffset = 0.25f;
    wt.setParams(p);
    wt.reset();

    auto [l, r] = wt.processSample();
    REQUIRE(l == Approx(1.f).margin(0.01f));
    REQUIRE(r == Approx(1.f).margin(0.01f));
}

//──────────────────────────────────────────────────────────────────────────────
TEST_CASE("GranularOsc cloud produces finite stereo grains", "[granular]")
{
    GranularOsc osc;
    osc.prepare(48000.0);
    osc.setFrequency(220.f);

    GranularOsc::Params p;
    p.triggerMode = GranularOsc::TriggerMode::FreeCloud;
    p.window = GranularOsc::Window::Hann;
    p.position = 0.42f;
    p.sizeMs = 80.f;
    p.density = 180.f;
    p.pitchSemis = 0.f;
    p.spray = 0.25f;
    p.jitter = 0.1f;
    p.panSpread = 1.f;
    osc.setParams(p);
    osc.noteOn(60, 0.8f);

    float sumL = 0.f;
    float sumR = 0.f;
    for (int i = 0; i < 4096; ++i)
    {
        auto [L, R] = osc.processSample();
        REQUIRE(std::isfinite(L));
        REQUIRE(std::isfinite(R));
        sumL += std::abs(L);
        sumR += std::abs(R);
    }

    REQUIRE(sumL > 0.01f);
    REQUIRE(sumR > 0.01f);
    REQUIRE(osc.getActiveGrainCount() > 0);
}

TEST_CASE("GranularOsc modes and windows stay finite under heavy density", "[granular]")
{
    for (int mode = 0; mode < static_cast<int>(GranularOsc::TriggerMode::NumModes); ++mode)
    {
        for (int win = 0; win < static_cast<int>(GranularOsc::Window::NumWindows); ++win)
        {
            GranularOsc osc;
            osc.prepare(48000.0);
            osc.setFrequency(440.f);

            GranularOsc::Params p;
            p.triggerMode = static_cast<GranularOsc::TriggerMode>(mode);
            p.window = static_cast<GranularOsc::Window>(win);
            p.sizeMs = 20.f;
            p.density = 900.f;
            p.pitchSemis = 7.f;
            p.spray = 0.8f;
            p.jitter = 0.5f;
            p.panSpread = 1.f;
            p.reverseProb = 0.5f;
            p.scanSpeed = 0.75f;
            osc.setParams(p);
            osc.noteOn(67, 1.f);

            for (int i = 0; i < 2048; ++i)
            {
                auto [L, R] = osc.processSample();
                REQUIRE(std::isfinite(L));
                REQUIRE(std::isfinite(R));
                REQUIRE(std::abs(L) <= 4.f);
                REQUIRE(std::abs(R) <= 4.f);
            }

            REQUIRE(osc.getActiveGrainCount() <= GranularOsc::kMaxGrains);
        }
    }
}

TEST_CASE("GranularOsc physical body bypass preserves pure granular output", "[granular]")
{
    GranularOsc dry;
    GranularOsc bypassed;
    dry.prepare(48000.0);
    bypassed.prepare(48000.0);
    dry.setFrequency(330.f);
    bypassed.setFrequency(330.f);

    GranularOsc::Params p;
    p.triggerMode = GranularOsc::TriggerMode::FreeCloud;
    p.window = GranularOsc::Window::Gaussian;
    p.sizeMs = 45.f;
    p.density = 240.f;
    p.spray = 0.35f;
    p.jitter = 0.2f;
    p.bodyType = GranularOsc::BodyType::Off;
    p.bodyMix = 0.f;
    dry.setParams(p);

    p.bodyType = GranularOsc::BodyType::Glass;
    p.bodyMix = 0.f;
    bypassed.setParams(p);

    dry.noteOn(64, 0.75f);
    bypassed.noteOn(64, 0.75f);

    for (int i = 0; i < 2048; ++i)
    {
        auto [dL, dR] = dry.processSample();
        auto [bL, bR] = bypassed.processSample();
        REQUIRE(bL == Approx(dL).margin(1.0e-6f));
        REQUIRE(bR == Approx(dR).margin(1.0e-6f));
    }
}

TEST_CASE("GranularOsc physical body types stay finite", "[granular]")
{
    for (int body = 0; body < static_cast<int>(GranularOsc::BodyType::NumBodyTypes); ++body)
    {
        GranularOsc osc;
        osc.prepare(48000.0);
        osc.setFrequency(196.f);

        GranularOsc::Params p;
        p.triggerMode = GranularOsc::TriggerMode::FreeCloud;
        p.window = GranularOsc::Window::Tukey;
        p.sizeMs = 35.f;
        p.density = 420.f;
        p.pitchSemis = -5.f;
        p.spray = 0.55f;
        p.jitter = 0.25f;
        p.panSpread = 1.f;
        p.bodyType = static_cast<GranularOsc::BodyType>(body);
        p.bodyRootSemis = -12.f;
        p.bodySize = 0.65f;
        p.bodyDecay = 0.8f;
        p.bodyDamping = 0.25f;
        p.bodyBrightness = 0.85f;
        p.bodyInharm = 0.6f;
        p.bodyExcite = 0.9f;
        p.bodyMix = body == 0 ? 0.f : 0.75f;
        p.bodySpread = 1.f;
        p.bodyModes = 32;
        osc.setParams(p);
        osc.noteOn(55, 1.f);

        for (int i = 0; i < 4096; ++i)
        {
            auto [L, R] = osc.processSample();
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            REQUIRE(std::abs(L) <= 4.f);
            REQUIRE(std::abs(R) <= 4.f);
        }
    }
}

TEST_CASE("FMOperator clamps shaping params and wraps high-rate phase", "[fm]")
{
    FMOperator op;
    op.prepare(48000.0);
    op.setNoteFrequency(440.f);

    ADSREnvelope::Params ep;
    ep.attackSec = 0.001f;
    ep.decaySec = 0.001f;
    ep.sustain = 1.f;
    ep.releaseSec = 0.001f;
    op.getEnvelope().setParams(ep);

    FMOperator::Params p;
    p.coarse = 31;
    p.level = 2.f;
    p.feedback = 2.f;
    p.velocityDepth = 2.f;
    p.keyScaleDepth = -2.f;
    p.waveShape = static_cast<FMOperator::Wave>(999);
    op.setParams(p);
    op.setVelocity(0.5f);
    op.setMidiNote(84);
    op.noteOn();

    float peak = 0.f;
    for (int i = 0; i < 2048; ++i)
    {
        float s = op.processSample(100.f);
        REQUIRE(std::isfinite(s));
        peak = std::max(peak, std::abs(s));
    }

    REQUIRE(peak <= 2.f);
}

TEST_CASE("FMOperator spectral waves stay finite", "[fm]")
{
    FMOperator op;
    op.prepare(48000.0);
    op.setNoteFrequency(220.f);
    op.setVelocity(1.f);
    op.setMidiNote(60);

    ADSREnvelope::Params ep;
    ep.attackSec = 0.001f;
    ep.decaySec = 0.001f;
    ep.sustain = 1.f;
    ep.releaseSec = 0.001f;
    op.getEnvelope().setParams(ep);

    for (int wave = 0; wave < (int)FMOperator::Wave::NumWaves; ++wave)
    {
        FMOperator::Params p;
        p.waveShape = static_cast<FMOperator::Wave>(wave);
        p.level = 1.f;
        p.feedback = 0.35f;
        p.spectralSkirt = 0.6f;
        p.spectralResonance = 0.7f;
        p.detune = 12.f;
        op.setParams(p);
        op.reset();
        op.noteOn();

        for (int i = 0; i < 2048; ++i)
        {
            float s = op.processSample(0.2f);
            REQUIRE(std::isfinite(s));
            REQUIRE(s >= -1.1f);
            REQUIRE(s <=  1.1f);
        }
    }
}

TEST_CASE("FM operators require noteOn and then produce output", "[fm]")
{
    FMOperator op;
    op.prepare(48000.0);
    op.setNoteFrequency(440.f);

    FMOperator::Params p;
    p.level = 1.f;
    op.setParams(p);

    float idleMax = 0.f;
    for (int i = 0; i < 128; ++i)
        idleMax = std::max(idleMax, std::abs(op.processSample(0.f)));
    REQUIRE(idleMax == Approx(0.f));

    op.noteOn();
    float activeMax = 0.f;
    for (int i = 0; i < 512; ++i)
        activeMax = std::max(activeMax, std::abs(op.processSample(0.f)));

    REQUIRE(activeMax > 0.01f);
}

TEST_CASE("FMAlgorithm processes operators once per sample", "[fm]")
{
    FMAlgorithm fm;
    fm.prepare(48000.0);
    fm.setNoteFrequency(440.f);
    fm.setAlgorithm(39);

    for (int i = 0; i < FMAlgorithm::kNumOperators; ++i)
    {
        FMOperator::Params p;
        p.level = i == 0 ? 1.f : 0.f;
        fm.getOperator(i).setParams(p);
    }

    fm.noteOn();

    float maxAbs = 0.f;
    for (int i = 0; i < 512; ++i)
        maxAbs = std::max(maxAbs, std::abs(fm.processSample()));

    REQUIRE(maxAbs > 0.001f);
    REQUIRE(maxAbs <= 1.f);
}

//──────────────────────────────────────────────────────────────────────────────
TEST_CASE("Arpeggiator honours played order separately from sorted patterns", "[arp]")
{
    Arpeggiator arp;
    arp.prepare(3200.0);

    Arpeggiator::Params p;
    p.pattern = Arpeggiator::Pattern::AsPlayed;
    p.rateIdx = 0;
    p.sync = false;
    p.gate = 0.5f;
    arp.setParams(p);

    arp.handleMidi(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100));
    arp.handleMidi(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100));
    arp.handleMidi(juce::MidiMessage::noteOn(1, 67, (juce::uint8)100));

    auto asPlayed = collectArpNoteOns(arp, 160);
    REQUIRE(asPlayed.size() >= 3);
    REQUIRE(asPlayed[0] == 64);
    REQUIRE(asPlayed[1] == 60);
    REQUIRE(asPlayed[2] == 67);

    arp.reset();
    p.pattern = Arpeggiator::Pattern::Up;
    arp.setParams(p);
    arp.handleMidi(juce::MidiMessage::noteOn(1, 67, (juce::uint8)100));
    arp.handleMidi(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100));
    arp.handleMidi(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100));

    auto sorted = collectArpNoteOns(arp, 160);
    REQUIRE(sorted.size() >= 3);
    REQUIRE(sorted[0] == 60);
    REQUIRE(sorted[1] == 64);
    REQUIRE(sorted[2] == 67);
}

TEST_CASE("Arpeggiator sync toggle chooses host tempo or internal tempo", "[arp]")
{
    Arpeggiator::Params p;
    p.pattern = Arpeggiator::Pattern::Up;
    p.rateIdx = 0;
    p.gate = 0.5f;

    Arpeggiator hostSynced;
    hostSynced.prepare(3200.0);
    hostSynced.setHostBPM(240.0);
    p.sync = true;
    hostSynced.setParams(p);
    hostSynced.handleMidi(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100));
    REQUIRE(!collectArpNoteOns(hostSynced, 30).empty());

    Arpeggiator freeRunning;
    freeRunning.prepare(3200.0);
    freeRunning.setHostBPM(240.0);
    p.sync = false;
    freeRunning.setParams(p);
    freeRunning.handleMidi(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100));
    REQUIRE(collectArpNoteOns(freeRunning, 30).empty());
}

TEST_CASE("Arpeggiator random latch repeats its shuffled order", "[arp]")
{
    Arpeggiator arp;
    arp.prepare(3200.0);

    Arpeggiator::Params p;
    p.pattern = Arpeggiator::Pattern::RandomLatch;
    p.rateIdx = 0;
    p.sync = false;
    p.gate = 0.5f;
    arp.setParams(p);

    arp.handleMidi(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100));
    arp.handleMidi(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100));
    arp.handleMidi(juce::MidiMessage::noteOn(1, 67, (juce::uint8)100));

    auto notes = collectArpNoteOns(arp, 320);
    REQUIRE(notes.size() >= 6);
    REQUIRE(notes[0] == notes[3]);
    REQUIRE(notes[1] == notes[4]);
    REQUIRE(notes[2] == notes[5]);
}
