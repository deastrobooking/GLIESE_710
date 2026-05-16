#include "PluginEditor.h"
#include "UI/IndustrialTheme.h"

GLIESE710AudioProcessorEditor::GLIESE710AudioProcessorEditor(GLIESE710AudioProcessor& p)
    : AudioProcessorEditor(&p),
      mainPanel(p.apvts, [&p](int terrainIndex, const juce::File& file) {
          return p.importTerrainHeightmap(terrainIndex, file);
      }),
      presetBrowser(p.presetManager),
      patchBrowser(p.presetManager)
{
    addAndMakeVisible(mainPanel);
    addAndMakeVisible(presetBrowser);
    addChildComponent(patchBrowser);   // starts hidden; shown via openBrowser()

    // Wire BROWSE button → open overlay
    presetBrowser.onBrowseClicked = [this] {
        patchBrowser.setBounds(getLocalBounds());
        patchBrowser.openBrowser();
    };

    // After the browser loads a preset, refresh the header combo box
    patchBrowser.onPresetLoaded = [this] {
        presetBrowser.refreshList();
    };

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

    // Keep the browser overlay covering the full editor (it positions itself)
    if (patchBrowser.isVisible())
        patchBrowser.setBounds(getLocalBounds());
}
