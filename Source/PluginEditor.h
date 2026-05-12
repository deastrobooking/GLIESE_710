#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MainPanel.h"
#include "UI/PresetBrowser.h"

class GLIESE710AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GLIESE710AudioProcessorEditor(GLIESE710AudioProcessor&);
    ~GLIESE710AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MainPanel    mainPanel;
    PresetBrowser presetBrowser;

    static constexpr int kDefaultWidth  = 1200;
    static constexpr int kDefaultHeight = 780;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLIESE710AudioProcessorEditor)
};
