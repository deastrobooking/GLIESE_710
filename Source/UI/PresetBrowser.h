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

    // Called when the user clicks the BROWSE button
    std::function<void()> onBrowseClicked;

private:
    PresetManager& presetManager;

    juce::ComboBox   presetBox;
    juce::TextButton saveButton  { "SAVE" };
    juce::TextButton prevButton  { "<" };
    juce::TextButton nextButton  { ">" };
    juce::TextButton browseButton{ "BROWSE" };
    juce::Label      nameLabel;

    int currentPresetIndex = -1;
    juce::Array<PresetManager::PatchInfo> presets;

    void saveCurrentPreset();
    void selectPreset(int index);
};
