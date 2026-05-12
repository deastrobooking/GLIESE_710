#include "PresetBrowser.h"

PresetBrowser::PresetBrowser(PresetManager& pm) : presetManager(pm)
{
    nameLabel.setText("GLIESE 710", juce::dontSendNotification);
    nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff88aaff));
    nameLabel.setFont(juce::Font(juce::FontOptions { 16.f, juce::Font::bold }));
    addAndMakeVisible(nameLabel);

    addAndMakeVisible(prevButton);
    prevButton.onClick = [this] {
        if (presetNames.isEmpty()) return;
        currentPresetIndex = (currentPresetIndex - 1 + presetNames.size()) % presetNames.size();
        selectPreset(currentPresetIndex);
    };

    addAndMakeVisible(nextButton);
    nextButton.onClick = [this] {
        if (presetNames.isEmpty()) return;
        currentPresetIndex = (currentPresetIndex + 1) % presetNames.size();
        selectPreset(currentPresetIndex);
    };

    presetBox.setJustificationType(juce::Justification::centred);
    presetBox.onChange = [this] {
        int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0 && idx < presetNames.size())
        {
            currentPresetIndex = idx;
            presetManager.loadPresetByName(presetNames[idx]);
        }
    };
    addAndMakeVisible(presetBox);

    saveButton.onClick = [this] { saveCurrentPreset(); };
    addAndMakeVisible(saveButton);

    refreshList();
}

void PresetBrowser::refreshList()
{
    presetNames = presetManager.getUserPresetNames();
    presetBox.clear(juce::dontSendNotification);
    for (int i = 0; i < presetNames.size(); ++i)
        presetBox.addItem(presetNames[i], i + 1);

    auto currentName = presetManager.getCurrentPresetName();
    int idx = presetNames.indexOf(currentName);
    if (idx >= 0)
    {
        currentPresetIndex = idx;
        presetBox.setSelectedItemIndex(idx, juce::dontSendNotification);
    }
}

void PresetBrowser::selectPreset(int index)
{
    if (index < 0 || index >= presetNames.size()) return;
    presetManager.loadPresetByName(presetNames[index]);
    presetBox.setSelectedItemIndex(index, juce::dontSendNotification);
}

void PresetBrowser::saveCurrentPreset()
{
    auto* window = new juce::AlertWindow("Save Preset", "Enter preset name:",
                                         juce::AlertWindow::NoIcon, this);
    window->addTextEditor("presetName", presetManager.getCurrentPresetName(), "Name");
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    window->enterModalState(
        true,
        juce::ModalCallbackFunction::create([this, window](int result) {
            if (result == 1)
            {
                auto name = window->getTextEditorContents("presetName").trim();
                if (name.isNotEmpty())
                {
                    presetManager.savePreset(name);
                    refreshList();
                }
            }

            delete window;
        }),
        false);
}

void PresetBrowser::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff0d0d1c));
    g.fillAll();
    g.setColour(juce::Colour(0xff3a3a5a));
    g.drawLine(0.f, (float)getHeight() - 1.f, (float)getWidth(), (float)getHeight() - 1.f, 1.f);
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(4, 2);
    nameLabel.setBounds(area.removeFromLeft(130));
    saveButton.setBounds(area.removeFromRight(60));
    nextButton.setBounds(area.removeFromRight(28));
    prevButton.setBounds(area.removeFromRight(28));
    area.removeFromLeft(4);
    presetBox.setBounds(area);
}
