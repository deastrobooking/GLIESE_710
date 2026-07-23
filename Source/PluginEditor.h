#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MainPanel.h"
#include "UI/PresetBrowser.h"
#include "UI/PatchBrowserPanel.h"

class OutputMeter final : public juce::Component,
                          public juce::SettableTooltipClient,
                          private juce::Timer
{
public:
    explicit OutputMeter(GLIESE710AudioProcessor& processor);
    void paint(juce::Graphics&) override;

private:
    GLIESE710AudioProcessor& processor;
    float peakL = 0.f;
    float peakR = 0.f;
    int clipHoldFrames = 0;
    void timerCallback() override;
};

class GLIESE710AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLIESE710AudioProcessorEditor(GLIESE710AudioProcessor&);
    ~GLIESE710AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MainPanel         mainPanel;
    PresetBrowser     presetBrowser;
    PatchBrowserPanel patchBrowser;
    OutputMeter       outputMeter;

    static constexpr int kDefaultWidth  = 1200;
    static constexpr int kDefaultHeight = 780;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLIESE710AudioProcessorEditor)
};
