#include <catch2/catch_all.hpp>
#include "../Source/Engine/Envelopes/ADSREnvelope.h"
#include "../Source/Engine/Oscillators/Osc3D.h"
#include "../Source/Engine/Oscillators/VAOscillator.h"
#include "../Source/Engine/Oscillators/WavetableOsc.h"
#include "../Source/Engine/Oscillators/GranularOsc.h"
#include "../Source/Engine/Oscillators/TerrainOsc.h"
#include "../Source/Engine/Oscillators/FMOperator.h"
#include "../Source/Engine/Oscillators/FMAlgorithm.h"
#include "../Source/Engine/Filter/DualFilter.h"
#include "../Source/Engine/Modulation/ModMatrix.h"
#include "../Source/Engine/Modulation/ModNames.h"
#include "../Source/Engine/Modulation/StepSequencerMod.h"
#include "../Source/Engine/LFO/LFOEngine.h"
#include "../Source/Engine/Arpeggiator/Arpeggiator.h"
#include <array>
#include <chrono>
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

class TestAudioProcessor final : public juce::AudioProcessor
{
public:
    TestAudioProcessor()
        : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override { return "Test"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
};

static juce::StringArray numberedChoices(int count)
{
    juce::StringArray choices;
    for (int i = 0; i < count; ++i)
        choices.add(juce::String(i));

    return choices;
}

static juce::AudioProcessorValueTreeState::ParameterLayout makeModSeqTestLayout()
{
    using RAP = juce::RangedAudioParameter;
    std::vector<std::unique_ptr<RAP>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { StepSequencerMod::kDepthParamID, 1 }, "Depth", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { StepSequencerMod::kDivisionParamID, 1 }, "Division",
        juce::StringArray { "1/4" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { StepSequencerMod::kSubdivisionParamID, 1 }, "Subdivision",
        juce::StringArray { "x1" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { StepSequencerMod::kLengthParamID, 1 }, "Length",
        juce::StringArray { "8", "16", "24", "32" }, 3));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { StepSequencerMod::kLoopLengthParamID, 1 }, "Loop Length",
        1, StepSequencerMod::kNumSteps, 3));

    const auto targetChoices = numberedChoices((int) ModDest::NumDests);
    for (int lane = 0; lane < StepSequencerMod::kNumLanes; ++lane)
    {
        const int target = lane == 0 ? (int) ModDest::FilterA_Cutoff : (int) ModDest::None;
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { StepSequencerMod::laneTargetParamID(lane), 1 },
            "Target", targetChoices, target));

        for (int step = 0; step < StepSequencerMod::kNumSteps; ++step)
        {
            float value = 0.0f;
            if (lane == 0)
                value = step == 0 ? 0.25f : (step == 1 ? 0.75f : (step == 2 ? 1.0f : 0.0f));

            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { StepSequencerMod::stepParamID(lane, step), 1 },
                "Step", 0.0f, 1.0f, value));
        }
    }

    return { params.begin(), params.end() };
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

    SECTION("field output is evaluated at audio rate")
    {
        Osc3D::Params p;
        p.field = Osc3D::FieldType::HarmonicLattice;
        p.path = Osc3D::PathType::Lissajous;
        p.fieldFreq = 8.f;
        p.warpAmt = 0.8f;
        p.zRatio = 2.5f;
        osc.setParams(p);
        osc.setFrequency(880.f);
        osc.reset();

        int heldAdjacentSamples = 0;
        float previous = osc.processSample();
        for (int i = 1; i < 64; ++i)
        {
            const float current = osc.processSample();
            if (current == previous)
                ++heldAdjacentSamples;
            previous = current;
        }

        REQUIRE(heldAdjacentSamples == 0);
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

    SECTION("all oscillator models stay finite")
    {
        for (int model = 0; model < (int)VAOscillator::Model::NumModels; ++model)
        {
            VAOscillator::Params p;
            p.model       = static_cast<VAOscillator::Model>(model);
            p.shapeMorph  = 1.7f;
            p.pulseWidth  = 0.42f;
            p.ringMod     = 0.35f;
            p.wavefold    = 0.25f;
            p.unisonCount = 5;
            p.detune      = 0.7f;
            p.spread      = 0.8f;
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

TEST_CASE("Modulation destination names cover appended backlog destinations", "[modmatrix]")
{
    const auto displayNames = ModNames::destDisplayNames();
    const auto parameterNames = ModNames::destParameterNames();

    REQUIRE(displayNames.size() == (int) ModDest::NumDests);
    REQUIRE(parameterNames.size() == (int) ModDest::NumDests);

    for (int i = 0; i < (int) ModDest::NumDests; ++i)
    {
        INFO("ModDest index " << i);
        REQUIRE(displayNames[i].isNotEmpty());
        REQUIRE(parameterNames[i].isNotEmpty());
    }

    REQUIRE(parameterNames[(int) ModDest::EngFiltTerrain_Mix] == "EngFiltTerrain_Mix");
    REQUIRE((int) ModDest::VA5_Shape > (int) ModDest::EngFiltTerrain_Mix);

    REQUIRE(parameterNames[(int) ModDest::VA5_Shape] == "VA5_Shape");
    REQUIRE(parameterNames[(int) ModDest::VA5_Level] == "VA5_Level");
    REQUIRE(parameterNames[(int) ModDest::VA6_Level] == "VA6_Level");
    REQUIRE(parameterNames[(int) ModDest::VA7_Level] == "VA7_Level");
    REQUIRE(parameterNames[(int) ModDest::VA8_Level] == "VA8_Level");
    REQUIRE(displayNames[(int) ModDest::VA8_Fold] == "VA8 Fold");

    REQUIRE((int) ModDest::Terrain_CenterX > (int) ModDest::VA8_Fold);
    REQUIRE(parameterNames[(int) ModDest::Terrain_CenterX] == "Terrain_CenterX");
    REQUIRE(parameterNames[(int) ModDest::Terrain_CenterY] == "Terrain_CenterY");
    REQUIRE(parameterNames[(int) ModDest::Terrain_RadiusX] == "Terrain_RadiusX");
    REQUIRE(parameterNames[(int) ModDest::Terrain_RadiusY] == "Terrain_RadiusY");
    REQUIRE(parameterNames[(int) ModDest::Terrain_RatioX] == "Terrain_RatioX");
    REQUIRE(parameterNames[(int) ModDest::Terrain_RatioY] == "Terrain_RatioY");
    REQUIRE(parameterNames[(int) ModDest::Terrain_Smoothing] == "Terrain_Smoothing");
    REQUIRE(parameterNames[(int) ModDest::Terrain_Morph] == "Terrain_Morph");
    REQUIRE(parameterNames[(int) ModDest::Terrain_Drive] == "Terrain_Drive");
    REQUIRE(displayNames[(int) ModDest::Terrain_Drive] == "Terrain Drive");
}

TEST_CASE("StepSequencer loop length trims playback without changing programmed length", "[modseq]")
{
    REQUIRE(StepSequencerMod::effectiveSequenceLength(32, 1) == 1);
    REQUIRE(StepSequencerMod::effectiveSequenceLength(16, 32) == 16);
    REQUIRE(StepSequencerMod::effectiveSequenceLength(32, 0) == 1);

    TestAudioProcessor processor;
    juce::AudioProcessorValueTreeState apvts(
        processor, nullptr, "State", makeModSeqTestLayout());

    StepSequencerMod seq;
    seq.prepare(48000.0);
    seq.setAPVTS(&apvts);

    seq.setHostPosition(2.1);
    seq.processBlock(64);
    REQUIRE(seq.getCurrentStep() == 2);
    REQUIRE(seq.getOutput().dests[(int) ModDest::FilterA_Cutoff] == Approx(1.0f));

    seq.setHostPosition(3.1);
    seq.processBlock(64);
    REQUIRE(seq.getCurrentStep() == 0);
    REQUIRE(seq.getOutput().dests[(int) ModDest::FilterA_Cutoff] == Approx(-0.5f));
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
        FilterMode::Formant,
        FilterMode::RolandJuno106,
        FilterMode::RolandJuno60,
        FilterMode::RolandJupiter4,
        FilterMode::RolandJupiter8,
        FilterMode::TB303,
        FilterMode::SequentialP5Rev2,
        FilterMode::SequentialP5Rev3,
        FilterMode::SequentialP600,
        FilterMode::SequentialP6,
        FilterMode::SequentialProOne,
        FilterMode::GliesePrism,
        FilterMode::AltLadder,
        FilterMode::PhysicalLadder
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

TEST_CASE("DualFilter resonance creates an audible response signature for every filter mode", "[filter]")
{
    constexpr double sampleRate = 48000.0;
    constexpr float cutoff = 950.f;
    constexpr int warmup = 2048;
    constexpr int n = 4096;
    constexpr std::array<float, 6> probeFrequencies {
        120.f, 360.f, 760.f, cutoff, 1500.f, 3600.f
    };

    auto measureRms = [](FilterMode mode, float resonance, float sineHz)
    {
        DualFilter df;
        df.prepare(48000.0);

        SingleFilter::Params p;
        p.mode = mode;
        p.cutoff = cutoff;
        p.resonance = resonance;
        p.drive = 0.08f;
        p.morph = 0.55f;
        p.saturation = 0.25f;
        p.bassComp = 0.25f;
        p.mix = 1.f;
        p.oversample = 0;
        p.envAmount = 0.f;

        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        float sumSq = 0.f;

        for (int i = 0; i < warmup + n; ++i)
        {
            const float t = (float) i / (float) sampleRate;
            const float input = 0.42f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * t);
            const float am = 0.55f * std::sin(2.f * juce::MathConstants<float>::pi * 137.f * t + 0.2f);

            const auto [left, right] = df.processSample(input, input, 0.f, 0.f, am);
            REQUIRE(std::isfinite(left));
            REQUIRE(std::isfinite(right));

            if (i >= warmup)
                sumSq += 0.5f * (left * left + right * right);
        }

        return std::sqrt(sumSq / (float) n);
    };

    for (int modeIndex = 0; modeIndex < static_cast<int>(FilterMode::NumModes); ++modeIndex)
    {
        const auto mode = static_cast<FilterMode>(modeIndex);

        float maxAbsDelta = 0.f;
        float maxRelativeDelta = 0.f;

        for (const float probeHz : probeFrequencies)
        {
            const float lowRes = measureRms(mode, 0.02f, probeHz);
            const float highRes = measureRms(mode, 0.92f, probeHz);
            const float delta = std::abs(highRes - lowRes);
            const float reference = std::max({ lowRes, highRes, 0.0005f });

            maxAbsDelta = std::max(maxAbsDelta, delta);
            maxRelativeDelta = std::max(maxRelativeDelta, delta / reference);
        }

        INFO("mode index: " << modeIndex);
        INFO("max absolute resonance delta: " << maxAbsDelta);
        INFO("max relative resonance delta: " << maxRelativeDelta);
        REQUIRE((maxAbsDelta > 0.0035f || maxRelativeDelta > 0.025f));
    }
}

TEST_CASE("SEM morph behaves as LP notch HP state-variable filter", "[filter]")
{
    auto measureRms = [](float sineHz, float morph)
    {
        DualFilter df;
        df.prepare(48000.0);

        SingleFilter::Params p;
        p.mode = FilterMode::SEMMorph;
        p.cutoff = 1000.f;
        p.resonance = 0.25f;
        p.drive = 0.f;
        p.morph = morph;
        p.saturation = 0.f;
        p.mix = 1.f;
        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        for (int i = 0; i < 2048; ++i)
        {
            const float s = std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            (void) df.processSample(s, s, 0.f, 0.f);
        }

        float sumSq = 0.f;
        constexpr int n = 8192;
        for (int i = 0; i < n; ++i)
        {
            const float s = std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            auto [L, R] = df.processSample(s, s, 0.f, 0.f);
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            sumSq += L * L;
        }

        return std::sqrt(sumSq / (float) n);
    };

    const float lpLow = measureRms(100.f, 0.f);
    const float lpHigh = measureRms(6000.f, 0.f);
    REQUIRE(lpLow > lpHigh * 8.f);

    const float hpLow = measureRms(100.f, 1.f);
    const float hpHigh = measureRms(6000.f, 1.f);
    REQUIRE(hpHigh > hpLow * 8.f);

    const float notchAtCutoff = measureRms(1000.f, 0.5f);
    const float notchBelow = measureRms(180.f, 0.5f);
    REQUIRE(notchBelow > notchAtCutoff * 2.f);
}

TEST_CASE("Roland filters behave as 24 dB low-pass models", "[filter]")
{
    const FilterMode modes[] {
        FilterMode::RolandJuno106,
        FilterMode::RolandJuno60,
        FilterMode::RolandJupiter4,
        FilterMode::RolandJupiter8
    };

    auto measureRms = [](FilterMode mode, float sineHz, float resonance)
    {
        DualFilter df;
        df.prepare(48000.0);

        SingleFilter::Params p;
        p.mode = mode;
        p.cutoff = 1100.f;
        p.resonance = resonance;
        p.drive = 0.1f;
        p.saturation = 0.25f;
        p.mix = 1.f;
        p.oversample = 1;
        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        for (int i = 0; i < 4096; ++i)
        {
            const float s = 0.5f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            (void) df.processSample(s, s, 0.f, 0.f);
        }

        float sumSq = 0.f;
        constexpr int n = 8192;
        for (int i = 0; i < n; ++i)
        {
            const float s = 0.5f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            auto [L, R] = df.processSample(s, s, 0.f, 0.f);
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            sumSq += L * L;
        }

        return std::sqrt(sumSq / (float) n);
    };

    for (auto mode : modes)
    {
        const float low = measureRms(mode, 180.f, 0.25f);
        const float high = measureRms(mode, 6000.f, 0.25f);
        REQUIRE(low > high * 6.f);
    }

    for (auto mode : modes)
    {
        const float flat = measureRms(mode, 900.f, 0.f);
        const float resonant = measureRms(mode, 900.f, 0.86f);
        INFO("mode index: " << static_cast<int>(mode));
        REQUIRE(resonant > flat * 1.2f);
    }

    for (auto mode : modes)
    {
        DualFilter df;
        df.prepare(48000.0);

        SingleFilter::Params p;
        p.mode = mode;
        p.cutoff = 650.f;
        p.resonance = 0.95f;
        p.drive = 0.75f;
        p.saturation = 0.85f;
        p.bassComp = 0.5f;
        p.mix = 1.f;
        p.oversample = 2;
        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        for (int i = 0; i < 2048; ++i)
        {
            const float impulse = i == 0 ? 1.f : 0.f;
            const float sine = 0.25f * std::sin(2.f * juce::MathConstants<float>::pi * 110.f * (float) i / 48000.f);
            auto [L, R] = df.processSample(impulse + sine, impulse + sine, 0.15f, -0.1f);
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            REQUIRE(std::abs(L) <= 1.5f);
            REQUIRE(std::abs(R) <= 1.5f);
        }
    }
}

TEST_CASE("TB303 filter behaves as acid 18 dB low-pass", "[filter]")
{
    auto measureRms = [](float sineHz, float morph, float resonance)
    {
        DualFilter df;
        df.prepare(48000.0);

        SingleFilter::Params p;
        p.mode = FilterMode::TB303;
        p.cutoff = 850.f;
        p.resonance = resonance;
        p.drive = 0.35f;
        p.morph = morph;
        p.saturation = 0.55f;
        p.mix = 1.f;
        p.oversample = 1;
        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        for (int i = 0; i < 4096; ++i)
        {
            const float s = 0.5f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            (void) df.processSample(s, s, 0.f, 0.f);
        }

        float sumSq = 0.f;
        constexpr int n = 8192;
        for (int i = 0; i < n; ++i)
        {
            const float s = 0.5f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            auto [L, R] = df.processSample(s, s, 0.f, 0.f);
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            sumSq += L * L;
        }

        return std::sqrt(sumSq / (float) n);
    };

    const float low = measureRms(180.f, 0.2f, 0.25f);
    const float high = measureRms(6000.f, 0.2f, 0.25f);
    REQUIRE(low > high * 4.f);

    const float unaccented = measureRms(2200.f, 0.f, 0.65f);
    const float accented = measureRms(2200.f, 1.f, 0.65f);
    REQUIRE(accented > unaccented * 1.2f);

    const float flatCutoff = measureRms(950.f, 0.35f, 0.f);
    const float resonantCutoff = measureRms(950.f, 0.35f, 0.86f);
    REQUIRE(resonantCutoff > flatCutoff * 1.25f);

    DualFilter df;
    df.prepare(48000.0);

    SingleFilter::Params p;
    p.mode = FilterMode::TB303;
    p.cutoff = 550.f;
    p.resonance = 0.98f;
    p.drive = 0.9f;
    p.morph = 1.f;
    p.saturation = 0.9f;
    p.bassComp = 0.35f;
    p.mix = 1.f;
    p.oversample = 2;
    df.setFilterA(p);
    df.setRouting(FilterRouting::AOnly);
    df.noteOn();

    for (int i = 0; i < 4096; ++i)
    {
        const float impulse = i == 0 ? 1.f : 0.f;
        const float saw = 0.4f * (2.f * std::fmod((float) i * 110.f / 48000.f, 1.f) - 1.f);
        auto [L, R] = df.processSample(impulse + saw, impulse + saw, 0.25f, -0.25f);
        REQUIRE(std::isfinite(L));
        REQUIRE(std::isfinite(R));
        REQUIRE(std::abs(L) <= 1.5f);
        REQUIRE(std::abs(R) <= 1.5f);
    }
}

TEST_CASE("GLIESE Prism filter morphs split resonant color", "[filter]")
{
    auto measureRms = [](float sineHz, float morph, float resonance)
    {
        DualFilter df;
        df.prepare(48000.0);

        SingleFilter::Params p;
        p.mode = FilterMode::GliesePrism;
        p.cutoff = 1200.f;
        p.resonance = resonance;
        p.drive = 0.25f;
        p.morph = morph;
        p.saturation = 0.45f;
        p.bassComp = 0.25f;
        p.mix = 1.f;
        p.oversample = 1;
        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        for (int i = 0; i < 4096; ++i)
        {
            const float s = 0.45f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            (void) df.processSample(s, s, 0.f, 0.f);
        }

        float sumSq = 0.f;
        constexpr int n = 8192;
        for (int i = 0; i < n; ++i)
        {
            const float s = 0.45f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            auto [L, R] = df.processSample(s, s, 0.f, 0.f);
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            sumSq += L * L;
        }

        return std::sqrt(sumSq / (float) n);
    };

    const float low = measureRms(220.f, 0.15f, 0.35f);
    const float high = measureRms(7000.f, 0.15f, 0.35f);
    REQUIRE(low > high * 2.5f);

    const float closedPrism = measureRms(1800.f, 0.05f, 0.72f);
    const float openPrism = measureRms(3000.f, 0.95f, 0.72f);
    REQUIRE(std::abs(openPrism - closedPrism) > 0.01f);

    const float flat = measureRms(1200.f, 0.65f, 0.f);
    const float resonant = measureRms(1200.f, 0.65f, 0.88f);
    REQUIRE(resonant > flat * 1.08f);
}

TEST_CASE("Alternate ladder filters behave as nonlinear low-pass models", "[filter]")
{
    auto measureRms = [](FilterMode mode, float sineHz, float morph, float resonance)
    {
        DualFilter df;
        df.prepare(48000.0);

        SingleFilter::Params p;
        p.mode = mode;
        p.cutoff = 1100.f;
        p.resonance = resonance;
        p.drive = 0.45f;
        p.morph = morph;
        p.saturation = 0.50f;
        p.bassComp = 0.25f;
        p.mix = 1.f;
        p.oversample = 1;
        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        for (int i = 0; i < 4096; ++i)
        {
            const float s = 0.42f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            (void) df.processSample(s, s, 0.f, 0.f);
        }

        float sumSq = 0.f;
        constexpr int n = 8192;
        for (int i = 0; i < n; ++i)
        {
            const float s = 0.42f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            auto [L, R] = df.processSample(s, s, 0.f, 0.f);
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            sumSq += L * L;
        }

        return std::sqrt(sumSq / (float) n);
    };

    for (auto mode : { FilterMode::AltLadder, FilterMode::PhysicalLadder })
    {
        const float low = measureRms(mode, 180.f, 0.70f, 0.45f);
        const float high = measureRms(mode, 6500.f, 0.70f, 0.45f);
        REQUIRE(low > high * 2.25f);

        const float flat = measureRms(mode, 1100.f, 0.75f, 0.05f);
        const float resonant = measureRms(mode, 1100.f, 0.75f, 0.88f);

        INFO("mode: " << static_cast<int>(mode));
        if (mode == FilterMode::AltLadder)
            REQUIRE(resonant > flat * 1.12f);
        else
            REQUIRE(std::abs(resonant - flat) > 0.01f);
    }
}

TEST_CASE("Sequential filters behave as SSM and CEM low-pass models", "[filter]")
{
    const FilterMode modes[] {
        FilterMode::SequentialP5Rev2,
        FilterMode::SequentialP5Rev3,
        FilterMode::SequentialP600,
        FilterMode::SequentialP6,
        FilterMode::SequentialProOne
    };

    auto measureRms = [](FilterMode mode, float sineHz, float morph, float resonance, float cutoff)
    {
        DualFilter df;
        df.prepare(48000.0);

        SingleFilter::Params p;
        p.mode = mode;
        p.cutoff = cutoff;
        p.resonance = resonance;
        p.drive = 0.22f;
        p.morph = morph;
        p.saturation = 0.35f;
        p.mix = 1.f;
        p.oversample = 1;
        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        for (int i = 0; i < 4096; ++i)
        {
            const float s = 0.45f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            (void) df.processSample(s, s, 0.f, 0.f);
        }

        float sumSq = 0.f;
        constexpr int n = 8192;
        for (int i = 0; i < n; ++i)
        {
            const float s = 0.45f * std::sin(2.f * juce::MathConstants<float>::pi * sineHz * (float) i / 48000.f);
            auto [L, R] = df.processSample(s, s, 0.f, 0.f);
            REQUIRE(std::isfinite(L));
            REQUIRE(std::isfinite(R));
            sumSq += L * L;
        }

        return std::sqrt(sumSq / (float) n);
    };

    for (auto mode : modes)
    {
        const float low = measureRms(mode, 180.f, 0.55f, 0.22f, 1000.f);
        const float high = measureRms(mode, 6000.f, 0.55f, 0.22f, 1000.f);
        REQUIRE(low > high * 5.f);
    }

    const float p600High = measureRms(FilterMode::SequentialP600, 4200.f, 0.5f, 0.2f, 1500.f);
    const float p5Rev3High = measureRms(FilterMode::SequentialP5Rev3, 4200.f, 0.5f, 0.2f, 1500.f);
    REQUIRE(p5Rev3High > p600High * 1.15f);

    const float p6Twelve = measureRms(FilterMode::SequentialP6, 6000.f, 0.f, 0.2f, 1200.f);
    const float p6TwentyFour = measureRms(FilterMode::SequentialP6, 6000.f, 1.f, 0.2f, 1200.f);
    REQUIRE(p6Twelve > p6TwentyFour * 1.4f);

    for (auto mode : modes)
    {
        DualFilter df;
        df.prepare(48000.0);

        SingleFilter::Params p;
        p.mode = mode;
        p.cutoff = 700.f;
        p.resonance = 0.97f;
        p.drive = 0.85f;
        p.morph = 0.8f;
        p.saturation = 0.85f;
        p.bassComp = 0.45f;
        p.mix = 1.f;
        p.oversample = 2;
        df.setFilterA(p);
        df.setRouting(FilterRouting::AOnly);
        df.noteOn();

        for (int i = 0; i < 2048; ++i)
        {
            const float impulse = i == 0 ? 1.f : 0.f;
            const float pulse = (i % 83) < 36 ? 0.35f : -0.35f;
            auto [L, R] = df.processSample(impulse + pulse, impulse + pulse, 0.2f, -0.2f);
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

TEST_CASE("GranularOsc position updates held cloud without retrigger", "[granular]")
{
    GranularOsc steady;
    GranularOsc moved;
    steady.prepare(48000.0);
    moved.prepare(48000.0);
    steady.setFrequency(220.f);
    moved.setFrequency(220.f);

    GranularOsc::Params p;
    p.triggerMode = GranularOsc::TriggerMode::FreeCloud;
    p.window = GranularOsc::Window::Hann;
    p.position = 0.08f;
    p.sizeMs = 35.f;
    p.density = 560.f;
    p.pitchSemis = 0.f;
    p.spray = 0.f;
    p.jitter = 0.f;
    p.panSpread = 0.f;
    p.scanSpeed = 0.f;
    p.freeze = 0.f;
    p.bodyType = GranularOsc::BodyType::Off;
    p.bodyMix = 0.f;

    steady.setParams(p);
    moved.setParams(p);
    steady.noteOn(60, 1.f);
    moved.noteOn(60, 1.f);

    for (int i = 0; i < 4096; ++i)
    {
        auto [sL, sR] = steady.processSample();
        auto [mL, mR] = moved.processSample();
        REQUIRE(mL == Approx(sL).margin(1.0e-6f));
        REQUIRE(mR == Approx(sR).margin(1.0e-6f));
    }

    auto movedParams = p;
    movedParams.position = 0.82f;
    moved.setParams(movedParams);

    float diff = 0.f;
    for (int i = 0; i < 12000; ++i)
    {
        auto [sL, sR] = steady.processSample();
        auto [mL, mR] = moved.processSample();
        diff += std::abs(mL - sL) + std::abs(mR - sR);
    }

    REQUIRE(diff > 0.05f);
}

TEST_CASE("GranularOsc default graintable tracks played pitch", "[granular]")
{
    auto estimatePitch = [](float targetHz)
    {
        GranularOsc osc;
        osc.prepare(48000.0);
        osc.setFrequency(targetHz);

        GranularOsc::Params p;
        p.triggerMode = GranularOsc::TriggerMode::FreeCloud;
        p.window = GranularOsc::Window::Hann;
        p.position = 0.f;
        p.sizeMs = 120.f;
        p.density = targetHz;
        p.pitchSemis = 0.f;
        p.spray = 0.f;
        p.jitter = 0.f;
        p.panSpread = 0.f;
        p.bodyType = GranularOsc::BodyType::Off;
        p.bodyMix = 0.f;
        osc.setParams(p);
        osc.noteOn(60, 1.f);

        for (int i = 0; i < 4800; ++i)
            (void) osc.processSample();

        constexpr int n = 12000;
        std::vector<float> samples;
        samples.reserve(n);
        float mean = 0.f;
        for (int i = 0; i < n; ++i)
        {
            auto [l, r] = osc.processSample();
            const float s = 0.5f * (l + r);
            samples.push_back(s);
            mean += s;
        }
        mean /= (float) n;
        for (auto& s : samples)
            s -= mean;

        const int minLag = juce::roundToInt(48000.f / (targetHz * 1.25f));
        const int maxLag = juce::roundToInt(48000.f / (targetHz * 0.75f));
        int bestLag = minLag;
        float bestCorr = -1.f;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            float corr = 0.f;
            float e0 = 0.f;
            float e1 = 0.f;
            for (int i = 0; i < n - lag; ++i)
            {
                const float a = samples[(size_t) i];
                const float b = samples[(size_t) (i + lag)];
                corr += a * b;
                e0 += a * a;
                e1 += b * b;
            }
            corr /= std::sqrt(juce::jmax(1.0e-12f, e0 * e1));
            if (corr > bestCorr)
            {
                bestCorr = corr;
                bestLag = lag;
            }
        }

        return 48000.f / (float) bestLag;
    };

    REQUIRE(estimatePitch(240.f) == Approx(240.f).margin(6.f));
    REQUIRE(estimatePitch(480.f) == Approx(480.f).margin(12.f));
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

TEST_CASE("TerrainOsc scan paths stay finite and audible", "[terrain]")
{
    for (int mode = 0; mode < static_cast<int>(TerrainOsc::PathMode::NumModes); ++mode)
    {
        TerrainOsc osc;
        osc.prepare(48000.0);
        osc.setFrequency(220.f);

        TerrainOsc::Params p;
        p.pathMode = static_cast<TerrainOsc::PathMode>(mode);
        p.centerX = 0.5f;
        p.centerY = 0.5f;
        p.radiusX = 0.38f;
        p.radiusY = 0.31f;
        p.ratioX = 1.0f + 0.25f * (float)mode;
        p.ratioY = 1.5f;
        p.spiral = 0.45f;
        p.drive = 2.0f;
        p.smoothing = 0.35f;
        p.morph = 0.25f;
        p.gain = 0.8f;
        osc.setParams(p);

        float peak = 0.f;
        for (int i = 0; i < 4096; ++i)
        {
            const float s = osc.processSample();
            REQUIRE(std::isfinite(s));
            REQUIRE(std::abs(s) <= 2.f);
            peak = std::max(peak, std::abs(s));
        }

        REQUIRE(peak > 0.01f);
    }
}

TEST_CASE("TerrainOsc imports brightness heightmap as bipolar terrain", "[terrain]")
{
    juce::Image image(juce::Image::RGB, 8, 8, true);
    for (int y = 0; y < image.getHeight(); ++y)
    {
        for (int x = 0; x < image.getWidth(); ++x)
        {
            const auto b = (juce::uint8)juce::jlimit(0, 255, x * 32 + y * 8);
            image.setPixelAt(x, y, juce::Colour(b, b, b));
        }
    }

    TerrainOsc osc;
    osc.prepare(48000.0);
    REQUIRE(osc.setHeightmapFromImage(image));
    REQUIRE(osc.getWidth() == 8);
    REQUIRE(osc.getHeight() == 8);
    osc.setFrequency(330.f);

    TerrainOsc::Params p;
    p.pathMode = TerrainOsc::PathMode::Line;
    p.centerX = 0.5f;
    p.centerY = 0.5f;
    p.radiusX = 0.45f;
    p.radiusY = 0.0f;
    p.drive = 1.0f;
    p.gain = 1.0f;
    osc.setParams(p);

    float minS = 1.f;
    float maxS = -1.f;
    for (int i = 0; i < 2048; ++i)
    {
        const float s = osc.processSample();
        REQUIRE(std::isfinite(s));
        minS = std::min(minS, s);
        maxS = std::max(maxS, s);
    }

    REQUIRE(maxS > minS + 0.05f);
}

TEST_CASE("TerrainOsc mipmap quality smooths high-rate checkerboard terrain", "[terrain]")
{
    juce::Image image(juce::Image::RGB, 64, 64, true);
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            image.setPixelAt(x, y, ((x + y) & 1) != 0 ? juce::Colours::white : juce::Colours::black);

    auto averageMagnitude = [&image](TerrainOsc::Quality quality)
    {
        TerrainOsc osc;
        osc.prepare(48000.0);
        REQUIRE(osc.setHeightmapFromImage(image));
        osc.setFrequency(9000.f);

        TerrainOsc::Params p;
        p.pathMode = TerrainOsc::PathMode::Line;
        p.quality = quality;
        p.centerX = 0.5f;
        p.centerY = 0.5f;
        p.radiusX = 0.5f;
        p.radiusY = 0.0f;
        p.ratioX = 1.0f;
        p.ratioY = 1.0f;
        p.scanRate = 8.0f;
        p.drive = 1.0f;
        p.gain = 1.0f;
        p.morph = 0.0f;
        p.smoothing = 0.0f;
        osc.setParams(p);

        float sum = 0.f;
        for (int i = 0; i < 2048; ++i)
        {
            const float s = osc.processSample();
            REQUIRE(std::isfinite(s));
            sum += std::abs(s);
        }

        return sum / 2048.f;
    };

    const float raw = averageMagnitude(TerrainOsc::Quality::Off);
    const float soft = averageMagnitude(TerrainOsc::Quality::Soft);

    REQUIRE(raw > 0.05f);
    REQUIRE(soft < raw * 0.5f);
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

TEST_CASE("FMOperator applies key-reset phase offset", "[fm]")
{
    FMOperator zero;
    FMOperator quarter;
    zero.prepare(48000.0);
    quarter.prepare(48000.0);

    ADSREnvelope::Params ep;
    ep.attackSec = 0.001f;
    ep.decaySec = 0.001f;
    ep.sustain = 1.f;
    ep.releaseSec = 0.001f;
    zero.getEnvelope().setParams(ep);
    quarter.getEnvelope().setParams(ep);

    FMOperator::Params p;
    p.level = 1.f;
    p.fixedFreq = true;
    p.fixedHz = 0.f;
    p.phaseOffset = 0.f;
    zero.setParams(p);
    p.phaseOffset = 0.25f;
    quarter.setParams(p);

    zero.noteOn();
    quarter.noteOn();

    REQUIRE(std::abs(zero.processSample(0.f)) < 1.0e-5f);
    REQUIRE(std::abs(quarter.processSample(0.f)) > 1.0e-4f);
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

TEST_CASE("FMAlgorithm algorithm morph remains finite", "[fm]")
{
    FMAlgorithm fm;
    fm.prepare(48000.0);
    fm.setNoteFrequency(110.f);
    fm.setAlgorithm(33);
    fm.setAlgorithmMorph(0.65f);

    for (int i = 0; i < FMAlgorithm::kNumOperators; ++i)
    {
        FMOperator::Params p;
        p.coarse = i + 1;
        p.level = 0.2f;
        p.feedback = 0.1f;
        fm.getOperator(i).setParams(p);
    }

    fm.noteOn();

    float maxAbs = 0.f;
    for (int i = 0; i < 1024; ++i)
    {
        const float s = fm.processSample();
        REQUIRE(std::isfinite(s));
        maxAbs = std::max(maxAbs, std::abs(s));
    }

    REQUIRE(maxAbs <= 2.f);
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

TEST_CASE("Realtime DSP hot paths survive a dense stress render", "[performance]")
{
    constexpr int samples = 8192;
    constexpr double sr = 48000.0;

    VAOscillator va;
    va.prepare(sr);
    va.setFrequency(110.f);
    VAOscillator::Params vap;
    vap.model = VAOscillator::Model::Sequential;
    vap.shapeMorph = 2.7f;
    vap.unisonCount = 8;
    vap.detune = 0.42f;
    vap.spread = 0.85f;
    vap.ringMod = 0.35f;
    vap.wavefold = 0.25f;
    va.setParams(vap);

    WavetableOsc wt;
    wt.prepare(sr);
    wt.setFrequency(165.f);
    WavetableOsc::Params wtp;
    wtp.warpMode = WavetableOsc::WarpMode::PDFormant;
    wtp.warpAmount = 0.65f;
    wtp.unison = 8;
    wtp.detune = 0.6f;
    wtp.spread = 0.9f;
    wtp.positionSpread = 0.4f;
    wt.setParams(wtp);
    wt.setTablePosition(4, 0.45f);

    GranularOsc gran;
    gran.prepare(sr);
    gran.setFrequency(82.41f);
    GranularOsc::Params gp;
    gp.triggerMode = GranularOsc::TriggerMode::FreeCloud;
    gp.window = GranularOsc::Window::Tukey;
    gp.sizeMs = 95.f;
    gp.density = 260.f;
    gp.jitter = 0.28f;
    gp.panSpread = 0.9f;
    gp.bodyType = GranularOsc::BodyType::Glass;
    gp.bodyMix = 0.35f;
    gp.bodyModes = 24;
    gran.setParams(gp);

    TerrainOsc terrain;
    terrain.prepare(sr);
    terrain.setFrequency(73.42f);
    TerrainOsc::Params tp;
    tp.pathMode = TerrainOsc::PathMode::OrbitNoise;
    tp.quality = TerrainOsc::Quality::Soft;
    tp.smoothing = 0.72f;
    tp.morph = 0.55f;
    tp.drive = 2.1f;
    terrain.setParams(tp);

    DualFilter filter;
    filter.prepare(sr);
    SingleFilter::Params fp;
    fp.mode = FilterMode::AltLadder;
    fp.cutoff = 2600.f;
    fp.resonance = 0.45f;
    fp.drive = 0.35f;
    fp.morph = 0.6f;
    fp.mix = 1.f;
    filter.setFilterA(fp);
    fp.mode = FilterMode::GliesePrism;
    fp.cutoff = 5100.f;
    fp.resonance = 0.38f;
    filter.setFilterB(fp);
    filter.setRouting(FilterRouting::Series);

    double energy = 0.0;
    float maxAbs = 0.f;
    const auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < samples; ++i)
    {
        auto [vaL, vaR] = va.processSample();
        auto [wtL, wtR] = wt.processSample();
        auto [gL, gR] = gran.processSample();
        const float t = terrain.processSample();
        auto [outL, outR] = filter.processSample(vaL + wtL + gL + t,
                                                 vaR + wtR + gR + t,
                                                 0.15f, -0.1f);

        REQUIRE(std::isfinite(outL));
        REQUIRE(std::isfinite(outR));
        maxAbs = std::max(maxAbs, std::max(std::abs(outL), std::abs(outR)));
        energy += (double) outL * (double) outL + (double) outR * (double) outR;
    }

    const auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    INFO("Stress render elapsed ms: " << elapsed);

    REQUIRE(maxAbs <= 8.f);
    REQUIRE(energy > 1.0e-5);
    REQUIRE(elapsed < 2000.0);
}
