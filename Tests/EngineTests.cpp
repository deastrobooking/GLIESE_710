#include <catch2/catch_all.hpp>
#include "../Source/Engine/Envelopes/ADSREnvelope.h"
#include "../Source/Engine/Oscillators/Osc3D.h"
#include "../Source/Engine/Oscillators/VAOscillator.h"
#include "../Source/Engine/Oscillators/WavetableOsc.h"
#include "../Source/Engine/Oscillators/FMOperator.h"
#include "../Source/Engine/Oscillators/FMAlgorithm.h"
#include "../Source/Engine/Filter/DualFilter.h"
#include "../Source/Engine/Modulation/ModMatrix.h"
#include "../Source/Engine/LFO/LFOEngine.h"
#include <vector>

using Catch::Approx;

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
    for (int mode = 0; mode < 12; ++mode)
    {
        SingleFilter::Params p;
        p.mode      = static_cast<FilterMode>(mode);
        p.cutoff    = 1000.f;
        p.resonance = 0.5f;
        p.drive     = 0.3f;
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
    wt.addTable("One", one.data(), (int)one.size());
    wt.addTable("MinusOne", minusOne.data(), (int)minusOne.size());

    wt.setTablePosition(1, 0.f);
    auto [oneL, oneR] = wt.processSample();
    REQUIRE(oneL == Approx(1.f).margin(0.001f));
    REQUIRE(oneR == Approx(1.f).margin(0.001f));

    wt.reset();
    wt.setTablePosition(2, 0.f);
    auto [minusL, minusR] = wt.processSample();
    REQUIRE(minusL == Approx(-1.f).margin(0.001f));
    REQUIRE(minusR == Approx(-1.f).margin(0.001f));
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
