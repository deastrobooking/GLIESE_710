#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "../Source/Engine/Envelopes/ADSREnvelope.h"
#include "../Source/Engine/Oscillators/Osc3D.h"
#include "../Source/Engine/Filter/DualFilter.h"
#include "../Source/Engine/Modulation/ModMatrix.h"

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

    // Unipolar: srcVal = 0.8, amount = 0.5 → accumulation = 0.4 (bipolar converts 0.8→0.6)
    REQUIRE(dst.dests[(int)ModDest::FilterA_Cutoff] != 0.f);
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
