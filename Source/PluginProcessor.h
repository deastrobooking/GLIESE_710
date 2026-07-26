#pragma once
#include <JuceHeader.h>
#include "Engine/SynthEngine.h"
#include "Engine/MasterOutputStage.h"
#include "Presets/PresetManager.h"

class GLIESE710AudioProcessor : public juce::AudioProcessor
{
public:
    GLIESE710AudioProcessor();
    ~GLIESE710AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool importTerrainHeightmap(int terrainIndex, const juce::File& file);
    void queueGranularSample(int granularIndex,
                             std::shared_ptr<const GranularOsc::SampleData> sample);
    std::shared_ptr<const GranularOsc::SampleData> captureGranularInput(
        int granularIndex, double seconds = 8.0);
    int getModSequencerCurrentStep() const noexcept { return synthEngine.getModSequencerCurrentStep(); }
    float getOutputPeakL() const noexcept { return outputPeakL.load(std::memory_order_relaxed); }
    float getOutputPeakR() const noexcept { return outputPeakR.load(std::memory_order_relaxed); }
    bool consumeOutputClipFlag() noexcept { return outputClip.exchange(false, std::memory_order_relaxed); }
    float getDspLoad() const noexcept { return dspLoad.load(std::memory_order_relaxed); }
    int getActiveVoiceCount() const noexcept { return activeVoiceCount.load(std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager;

private:
    SynthEngine synthEngine;
    MasterOutputStage masterOutput;
    std::atomic<float> outputPeakL { 0.f };
    std::atomic<float> outputPeakR { 0.f };
    std::atomic<bool> outputClip { false };
    std::atomic<float> dspLoad { 0.f };
    std::atomic<int> activeVoiceCount { 0 };
    std::vector<float> inputCaptureL, inputCaptureR;
    int inputCaptureWrite = 0;
    int inputCaptureValid = 0;
    juce::SpinLock inputCaptureLock;
    void recordInput(const juce::AudioBuffer<float>& buffer);
    void restoreTerrainHeightmapsFromState();
    void restoreGranularSamplesFromState();
    void restoreExternalAssetsFromState();
    static juce::Identifier terrainStateProperty(int terrainIndex);
    static juce::Identifier granularStateProperty(int granularIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLIESE710AudioProcessor)
};
