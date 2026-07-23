#include "PluginEditor.h"
#include "UI/IndustrialTheme.h"

OutputMeter::OutputMeter(GLIESE710AudioProcessor& p) : processor(p)
{
    setTooltip("Stereo master output level. Red indicates limiter activity or recovered invalid audio.");
    startTimerHz(30);
}

void OutputMeter::timerCallback()
{
    peakL = processor.getOutputPeakL();
    peakR = processor.getOutputPeakR();
    if (processor.consumeOutputClipFlag())
        clipHoldFrames = 30;
    else if (clipHoldFrames > 0)
        --clipHoldFrames;
    repaint();
}

void OutputMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(5, 7);
    g.setColour(juce::Colour(0xff090c10));
    g.fillRoundedRectangle(bounds.toFloat(), 3.f);

    auto labelArea = bounds.removeFromLeft(24);
    g.setColour(IndustrialTheme::text().withAlpha(0.65f));
    g.setFont(juce::Font(juce::FontOptions { 8.f, juce::Font::bold }));
    g.drawText("OUT", labelArea, juce::Justification::centred);

    auto drawChannel = [&](juce::Rectangle<int> area, float peak) {
        g.setColour(juce::Colour(0xff20262d));
        g.fillRoundedRectangle(area.toFloat(), 1.5f);
        const float normalized = juce::jlimit(0.f, 1.f, peak / 0.98f);
        auto level = area.toFloat();
        level.setWidth(level.getWidth() * normalized);
        const auto colour = normalized > 0.9f ? IndustrialTheme::accentMaster()
                                              : juce::Colour(0xff51d88a);
        g.setColour(colour);
        g.fillRoundedRectangle(level, 1.5f);
    };

    auto rows = bounds;
    drawChannel(rows.removeFromTop(8), peakL);
    rows.removeFromTop(3);
    drawChannel(rows.removeFromTop(8), peakR);

    if (clipHoldFrames > 0)
    {
        g.setColour(IndustrialTheme::accentMaster());
        g.fillEllipse((float) getWidth() - 8.f, 4.f, 4.f, 4.f);
    }
}

GLIESE710AudioProcessorEditor::GLIESE710AudioProcessorEditor(GLIESE710AudioProcessor& p)
    : AudioProcessorEditor(&p),
      mainPanel(p.apvts, [&p](int terrainIndex, const juce::File& file) {
          return p.importTerrainHeightmap(terrainIndex, file);
      }, [&p] {
          return p.getModSequencerCurrentStep();
      }),
      presetBrowser(p.presetManager),
      patchBrowser(p.presetManager),
      outputMeter(p)
{
    addAndMakeVisible(mainPanel);
    addAndMakeVisible(presetBrowser);
    addAndMakeVisible(outputMeter);
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
    auto header = area.removeFromTop(36);
    outputMeter.setBounds(header.removeFromRight(112));
    presetBrowser.setBounds(header);
    mainPanel.setBounds(area);

    // Keep the browser overlay covering the full editor (it positions itself)
    if (patchBrowser.isVisible())
        patchBrowser.setBounds(getLocalBounds());
}
