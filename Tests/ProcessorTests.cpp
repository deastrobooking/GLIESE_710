#include <catch2/catch_all.hpp>
#include "../Source/PluginProcessor.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
void setParameter(GLIESE710AudioProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.apvts.getParameter(id);
    REQUIRE(parameter != nullptr);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void loadInit(GLIESE710AudioProcessor& processor)
{
    for (const auto& patch : processor.presetManager.getFactoryPatches())
        if (patch.name == "Init")
        {
            REQUIRE(processor.presetManager.loadPreset(patch));
            return;
        }
    FAIL("Init preset missing");
}

void configureSolo(GLIESE710AudioProcessor& processor, int type)
{
    for (const auto* page : { "a", "b" })
        for (int slot = 0; slot < 4; ++slot)
            setParameter(processor, "engine_block_" + juce::String(page) + "_slot_"
                                      + juce::String(slot), 0.f);
    setParameter(processor, "engine_block_a_slot_0", (float)type);
    setParameter(processor, "filter_a_mix", 0.f);
    setParameter(processor, "filter_b_mix", 0.f);
    setParameter(processor, StepSequencerMod::kDepthParamID, 0.f);
    setParameter(processor, "master_adsr_a", 0.001f);
    setParameter(processor, "master_adsr_d", 0.001f);
    setParameter(processor, "master_adsr_s", 1.f);
    setParameter(processor, "master_adsr_r", 0.001f);
    if (type == 2)
    {
        setParameter(processor, "osc_va_shape", 0.f);
        setParameter(processor, "osc_va_level", 0.6f);
    }
    else if (type == 5)
    {
        setParameter(processor, "osc_grain_level", 0.6f);
        setParameter(processor, "osc_grain_layer_morph", 0.5f);
    }
    else if (type == 6)
        setParameter(processor, "osc_terrain_level", 0.6f);
}

std::vector<float> render(GLIESE710AudioProcessor& processor, int samples,
                          int blockSize, const juce::MidiMessage* initialEvent = nullptr)
{
    std::vector<float> result;
    result.reserve((size_t)samples);
    for (int offset = 0; offset < samples; offset += blockSize)
    {
        const int count = std::min(blockSize, samples - offset);
        juce::AudioBuffer<float> buffer(2, count);
        buffer.clear();
        juce::MidiBuffer midi;
        if (offset == 0 && initialEvent != nullptr)
            midi.addEvent(*initialEvent, 0);
        processor.processBlock(buffer, midi);
        for (int sample = 0; sample < count; ++sample)
        {
            const auto value = buffer.getSample(0, sample);
            REQUIRE(std::isfinite(value));
            result.push_back(value);
        }
    }
    return result;
}
}

TEST_CASE("Preset recall clears absent granular and terrain assets", "[processor][preset][regression]")
{
    auto expected = std::make_unique<GLIESE710AudioProcessor>();
    auto restored = std::make_unique<GLIESE710AudioProcessor>();
    expected->prepareToPlay(48000, 128);
    restored->prepareToPlay(48000, 128);
    int engine = 5;
    SECTION("granular layers")
    {
        std::vector<float> sample(8192);
        for (size_t i = 0; i < sample.size(); ++i)
            sample[i] = std::sin((float)i * 0.13f);
        restored->queueGranularSample(0, 0, GranularOsc::createSampleData(sample.data(), (int)sample.size()));
        std::reverse(sample.begin(), sample.end());
        restored->queueGranularSample(0, 1, GranularOsc::createSampleData(sample.data(), (int)sample.size()));
    }
    SECTION("terrain map")
    {
        engine = 6;
        juce::Image image(juce::Image::RGB, 16, 16, true);
        juce::Graphics graphics(image);
        graphics.fillAll(juce::Colours::white);
        juce::TemporaryFile file(".png");
        {
            juce::FileOutputStream stream(file.getFile());
            REQUIRE(juce::PNGImageFormat().writeImageToStream(image, stream));
        }
        REQUIRE(restored->importTerrainHeightmap(0, file.getFile()));
    }
    loadInit(*expected);
    loadInit(*restored);
    configureSolo(*expected, engine);
    configureSolo(*restored, engine);
    auto note = juce::MidiMessage::noteOn(1, 60, (juce::uint8)100);
    const auto a = render(*expected, 8192, 128, &note);
    const auto b = render(*restored, 8192, 128, &note);
    float difference = 0.f;
    float energy = 0.f;
    for (size_t i = 0; i < a.size(); ++i)
    {
        difference = std::max(difference, std::abs(a[i] - b[i]));
        energy += a[i] * a[i];
    }
    REQUIRE(energy > 0.001f);
    REQUIRE(difference < 0.00001f);
}

TEST_CASE("Host state recall restores defaults and rejects unrelated XML", "[processor][preset][regression]")
{
    auto processor = std::make_unique<GLIESE710AudioProcessor>();
    setParameter(*processor, "filter_a_cutoff", 777.f);
    juce::MemoryBlock binary;
    juce::XmlElement empty("State");
    juce::AudioProcessor::copyXmlToBinary(empty, binary);
    processor->setStateInformation(binary.getData(), (int)binary.getSize());
    REQUIRE(processor->apvts.getRawParameterValue("filter_a_cutoff")->load() == Catch::Approx(20000.f));
    juce::XmlElement unrelated("Unrelated");
    juce::AudioProcessor::copyXmlToBinary(unrelated, binary);
    processor->setStateInformation(binary.getData(), (int)binary.getSize());
    REQUIRE(processor->apvts.state.hasType("State"));
}

TEST_CASE("Arpeggiator mode changes release notes and discard latched keys", "[processor][arp][regression]")
{
    auto processor = std::make_unique<GLIESE710AudioProcessor>();
    processor->prepareToPlay(48000, 128);
    configureSolo(*processor, 2);
    setParameter(*processor, "arp_on", 1.f);
    setParameter(*processor, "arp_latch", 1.f);
    setParameter(*processor, "arp_gate", 1.f);
    setParameter(*processor, "arp_rate", 0.f);
    const auto note = juce::MidiMessage::noteOn(1, 60, (juce::uint8)100);
    render(*processor, 4800, 128, &note);
    REQUIRE(processor->getActiveVoiceCount() > 0);
    setParameter(*processor, "arp_on", 0.f);
    render(*processor, 48000, 128);
    REQUIRE(processor->getActiveVoiceCount() == 0);
    setParameter(*processor, "arp_on", 1.f);
    render(*processor, 9600, 128);
    REQUIRE(processor->getActiveVoiceCount() == 0);
}

TEST_CASE("Portamento reaches the new note independently of host block size", "[processor][regression]")
{
    for (int blockSize : { 64, 512 })
    {
        auto processor = std::make_unique<GLIESE710AudioProcessor>();
        processor->prepareToPlay(48000, blockSize);
        configureSolo(*processor, 2);
        const auto first = juce::MidiMessage::noteOn(1, 60, (juce::uint8)100);
        render(*processor, 4800, blockSize, &first);
        const auto off = juce::MidiMessage::noteOff(1, 60);
        render(*processor, 48000, blockSize, &off);
        REQUIRE(processor->getActiveVoiceCount() == 0);
        setParameter(*processor, "master_portamento", 0.1f);
        const auto second = juce::MidiMessage::noteOn(1, 72, (juce::uint8)100);
        const auto output = render(*processor, 24000, blockSize, &second);
        std::vector<int> crossings;
        for (int i = (int)output.size() - 2048; i + 1 < (int)output.size(); ++i)
            if (output[(size_t)i] <= 0.f && output[(size_t)i + 1] > 0.f)
                crossings.push_back(i);
        REQUIRE(crossings.size() >= 2);
        const double frequency = 48000.0 * (double)(crossings.size() - 1)
            / (double)(crossings.back() - crossings.front());
        REQUIRE(frequency > 510.0);
        REQUIRE(frequency < 525.0);
    }
}
