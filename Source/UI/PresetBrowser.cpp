#include "PresetBrowser.h"
#include "IndustrialTheme.h"

PresetBrowser::PresetBrowser(PresetManager& pm) : presetManager(pm)
{
    auto accent = IndustrialTheme::accentMaster();

    nameLabel.setText("GLIESE 710", juce::dontSendNotification);
    IndustrialTheme::styleTitle(nameLabel, accent, 16.f);
    addAndMakeVisible(nameLabel);

    IndustrialTheme::styleButton(prevButton, accent);
    addAndMakeVisible(prevButton);
    prevButton.onClick = [this] {
        if (presets.isEmpty()) return;
        currentPresetIndex = (currentPresetIndex - 1 + presets.size()) % presets.size();
        selectPreset(currentPresetIndex);
    };

    IndustrialTheme::styleButton(nextButton, accent);
    addAndMakeVisible(nextButton);
    nextButton.onClick = [this] {
        if (presets.isEmpty()) return;
        currentPresetIndex = (currentPresetIndex + 1) % presets.size();
        selectPreset(currentPresetIndex);
    };

    IndustrialTheme::styleCombo(presetBox, accent);
    presetBox.onChange = [this] {
        int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx < presets.size())
        {
            currentPresetIndex = idx;
            presetManager.loadPreset(presets.getReference(idx));
        }
    };
    addAndMakeVisible(presetBox);

    IndustrialTheme::styleButton(saveButton, accent);
    saveButton.onClick = [this] { saveCurrentPreset(); };
    addAndMakeVisible(saveButton);

    IndustrialTheme::styleButton(browseButton, accent);
    browseButton.onClick = [this] { if (onBrowseClicked) onBrowseClicked(); };
    addAndMakeVisible(browseButton);

    refreshList();
}

void PresetBrowser::refreshList()
{
    presets = presetManager.getAllPatches();
    presetBox.clear(juce::dontSendNotification);
    for (int i = 0; i < presets.size(); ++i)
    {
        const auto& preset = presets.getReference(i);
        presetBox.addItem(preset.isFactory ? "◆ " + preset.name : preset.name, i + 1);
    }

    auto currentName = presetManager.getCurrentPresetName();
    int idx = -1;
    for (int i = 0; i < presets.size(); ++i)
        if (presets.getReference(i).name == currentName)
        {
            idx = i;
            break;
        }
    if (idx >= 0)
    {
        currentPresetIndex = idx;
        presetBox.setSelectedItemIndex(idx, juce::dontSendNotification);
    }
}

void PresetBrowser::selectPreset(int index)
{
    if (index < 0 || index >= presets.size()) return;
    presetManager.loadPreset(presets.getReference(index));
    presetBox.setSelectedItemIndex(index, juce::dontSendNotification);
}

void PresetBrowser::saveCurrentPreset()
{
    auto* window = new juce::AlertWindow("Save Preset", "Enter preset name:",
                                         juce::AlertWindow::NoIcon, nullptr);
    window->addTextEditor("presetName", presetManager.getCurrentPresetName(), "Name");
    window->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(
        true,
        juce::ModalCallbackFunction::create([safeThis = juce::Component::SafePointer<PresetBrowser>(this), window](int result) {
            if (safeThis == nullptr)
            {
                delete window;
                return;
            }
            if (result == 1)
            {
                auto name = window->getTextEditorContents("presetName").trim();
                if (name.isNotEmpty())
                {
                    if (safeThis->presetManager.savePreset(name))
                    {
                        safeThis->refreshList();
                    }
                    else
                    {
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                            "Save Failed", "The preset could not be written. Check the name and destination permissions.");
                    }
                }
            }
            delete window;
        }),
        false);
}

void PresetBrowser::paint(juce::Graphics& g)
{
    IndustrialTheme::paintFaceplate(g, getLocalBounds());
    g.setColour(IndustrialTheme::accentMaster().withAlpha(0.42f));
    g.drawLine(0.f, (float)getHeight() - 1.f, (float)getWidth(), (float)getHeight() - 1.f, 1.f);
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(4, 2);
    nameLabel   .setBounds(area.removeFromLeft(130));
    browseButton.setBounds(area.removeFromLeft(70).reduced(2, 0));
    area.removeFromLeft(4);
    saveButton  .setBounds(area.removeFromRight(60));
    nextButton  .setBounds(area.removeFromRight(28));
    prevButton  .setBounds(area.removeFromRight(28));
    area.removeFromLeft(4);
    presetBox   .setBounds(area);
}
