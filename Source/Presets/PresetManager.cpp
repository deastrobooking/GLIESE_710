#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    presetsDir = getPresetsDirectory();
    ensureDirectoryExists();
}

void PresetManager::ensureDirectoryExists()
{
    if (!presetsDir.exists())
        presetsDir.createDirectory();
}

juce::File PresetManager::getPresetsDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory)
               .getChildFile(kPresetDirName)
               .getChildFile("Presets");
}

juce::ValueTree PresetManager::stateToValueTree() const
{
    return apvts.copyState();
}

void PresetManager::valueTreeToState(const juce::ValueTree& tree)
{
    apvts.replaceState(tree);
}

void PresetManager::savePreset(const juce::String& name)
{
    ensureDirectoryExists();
    auto file = presetsDir.getChildFile(name + kPresetExtension);

    auto state = stateToValueTree();
    if (auto xml = state.createXml())
    {
        xml->writeToFile(file, {});
        currentPresetName = name;
    }
}

bool PresetManager::loadPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid())
        {
            valueTreeToState(tree);
            currentPresetName = file.getFileNameWithoutExtension();
            return true;
        }
    }
    return false;
}

bool PresetManager::loadPresetByName(const juce::String& name)
{
    return loadPreset(presetsDir.getChildFile(name + kPresetExtension));
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (auto& f : presetsDir.findChildFiles(juce::File::findFiles, false, "*" + juce::String(kPresetExtension)))
        names.add(f.getFileNameWithoutExtension());
    names.sort(false);
    return names;
}
