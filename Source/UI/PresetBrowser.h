#pragma once
#include <JuceHeader.h>
#include "../Presets/PresetManager.h"

class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser(PresetManager& pm);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshList();

private:
    PresetManager& presetManager;

    juce::ComboBox presetBox;
    juce::TextButton saveButton  { "Save" };
    juce::TextButton prevButton  { "<" };
    juce::TextButton nextButton  { ">" };
    juce::Label      nameLabel;

    int currentPresetIndex = -1;
    juce::StringArray presetNames;

    void saveCurrentPreset();
    void selectPreset(int index);
};
