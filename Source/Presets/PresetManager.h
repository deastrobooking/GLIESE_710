#pragma once
#include <JuceHeader.h>

class PresetManager
{
public:
    static constexpr const char* kPresetExtension = ".g710";
    static constexpr const char* kPresetDirName   = "GLIESE_710";

    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    void savePreset(const juce::String& name);
    bool loadPreset(const juce::File& file);
    bool loadPresetByName(const juce::String& name);

    juce::StringArray getUserPresetNames() const;
    juce::File        getPresetsDirectory() const;

    const juce::String& getCurrentPresetName() const { return currentPresetName; }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentPresetName { "Init" };

    juce::File presetsDir;

    void ensureDirectoryExists();
    juce::ValueTree stateToValueTree() const;
    void valueTreeToState(const juce::ValueTree& tree);
};
