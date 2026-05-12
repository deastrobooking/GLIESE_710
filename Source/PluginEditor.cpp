#include "PluginEditor.h"
#include "UI/IndustrialTheme.h"

GLIESE710AudioProcessorEditor::GLIESE710AudioProcessorEditor(GLIESE710AudioProcessor& p)
    : AudioProcessorEditor(&p),
      mainPanel(p.apvts),
      presetBrowser(p.presetManager)
{
    addAndMakeVisible(mainPanel);
    addAndMakeVisible(presetBrowser);

    setResizable(true, true);
    setResizeLimits(900, 600, 1920, 1200);
    setSize(kDefaultWidth, kDefaultHeight);
}

GLIESE710AudioProcessorEditor::~GLIESE710AudioProcessorEditor() = default;

void GLIESE710AudioProcessorEditor::paint(juce::Graphics& g)
{
    IndustrialTheme::paintFaceplate(g, getLocalBounds());
}

void GLIESE710AudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    presetBrowser.setBounds(area.removeFromTop(36));
    mainPanel.setBounds(area);
}
