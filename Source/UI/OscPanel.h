#pragma once
#include <JuceHeader.h>
#include "IndustrialTheme.h"
#include "../Engine/Oscillators/Osc3D.h"
#include "../Engine/Oscillators/GranularOsc.h"

// Draws the current VA waveform shape — updated via setShape()
class VAWaveDisplay : public juce::Component
{
public:
    explicit VAWaveDisplay(juce::Colour accentColour) : accent(accentColour) {}

    void setShape(int shapeIndex)
    {
        if (shape == shapeIndex) return;
        shape = shapeIndex;
        repaint();
    }

    void paint(juce::Graphics& g) override;

private:
    int          shape  = 0;
    juce::Colour accent;
};

class TerrainHeightmapDisplay : public juce::Component,
                                private juce::Timer
{
public:
    struct ScanState
    {
        int pathMode = 0;
        float centerX = 0.5f;
        float centerY = 0.5f;
        float radiusX = 0.35f;
        float radiusY = 0.35f;
        float zoom = 1.0f;
        float rotation = 0.0f;
        float pathSize = 1.0f;
        float ratioX = 1.0f;
        float ratioY = 1.5f;
        float scanRate = 1.0f;
        float phaseOffset = 0.0f;
        float spiral = 0.0f;
    };

    explicit TerrainHeightmapDisplay(juce::Colour accentColour);

    void setHeightmapImage(const juce::Image& image);
    void setScanState(const ScanState& newState);
    void paint(juce::Graphics& g) override;

private:
    juce::Colour accent;
    juce::Image heightmapPreview;
    juce::Image proceduralPreview;
    ScanState state;

    void timerCallback() override;
    void rebuildProceduralPreview();
    std::pair<float, float> pointFor(float t) const noexcept;
    static float wrap01(float v) noexcept;
    static float noise(float x, float y) noexcept;
};

class Osc3DFieldDisplay : public juce::Component,
                          private juce::Timer
{
public:
    Osc3DFieldDisplay(juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& parameterPrefix,
                      juce::Colour accentColour);
    void paint(juce::Graphics&) override;

private:
    juce::Colour accent;
    Osc3D previewOsc;
    std::atomic<float>* field = nullptr;
    std::atomic<float>* path = nullptr;
    std::atomic<float>* radius = nullptr;
    std::atomic<float>* depth = nullptr;
    std::atomic<float>* warp = nullptr;
    std::atomic<float>* rotation = nullptr;
    std::atomic<float>* chaos = nullptr;
    std::atomic<float>* morph = nullptr;
    std::atomic<float>* fieldFreq = nullptr;
    std::atomic<float>* zRatio = nullptr;
    std::atomic<float>* fold = nullptr;
    std::atomic<float>* drive = nullptr;

    void timerCallback() override;
    float value(std::atomic<float>* parameter, float fallback) const noexcept;
    juce::Point<float> project(Vec3 point, juce::Rectangle<float> bounds) const noexcept;
    Vec3 pathPoint(float phase, const Osc3D::Params& params) const noexcept;
};

class GranularWaveformDisplay : public juce::Component
{
public:
    explicit GranularWaveformDisplay(juce::Colour accentColour) : accent(accentColour) {}
    void setSample(std::shared_ptr<const GranularOsc::SampleData> newSample,
                   juce::String newName);
    void setRegion(float start, float end, int slices, int selectedSlice,
                   bool useTransientSlices);
    void paint(juce::Graphics&) override;

private:
    juce::Colour accent;
    std::shared_ptr<const GranularOsc::SampleData> sample;
    juce::String sampleName { "DROP AUDIO" };
    float regionStart = 0.f, regionEnd = 1.f;
    int sliceCount = 1, sliceIndex = 0;
    bool transientSlices = false;
};

// Panel for one oscillator section (3D, WT, VA, KS, granular, or terrain)
class OscPanel : public juce::Component,
                 public juce::FileDragAndDropTarget
{
public:
    using TerrainImportCallback = std::function<bool(int terrainIndex, const juce::File& file)>;
    using GranularImportCallback = std::function<void(
        int granularIndex, std::shared_ptr<const GranularOsc::SampleData>)>;

    explicit OscPanel(juce::AudioProcessorValueTreeState& apvts, const juce::String& oscId,
                      const juce::String& label,
                      TerrainImportCallback terrainImport = {},
                      int terrainIndex = -1,
                      GranularImportCallback granularImport = {},
                      int granularIndex = -1);

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int, int) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String oscId;
    juce::String label;
    juce::Colour accent;
    TerrainImportCallback terrainImport;
    int terrainIndex = -1;
    GranularImportCallback granularImport;
    int granularIndex = -1;

    juce::Label titleLabel;

    using Att  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Engine enable / hard shutoff
    juce::ToggleButton enableButton;
    std::unique_ptr<BAtt> enableAtt;
    juce::ToggleButton retrigButton;
    std::unique_ptr<BAtt> retrigAtt;
    bool hasRetrigButton = false;
    juce::TextButton importHeightmapButton { "IMPORT MAP" };
    juce::TextButton importSampleButton { "LOAD SAMPLE" };
    bool hasTerrainImportButton = false;
    bool hasGranularImportButton = false;
    bool terrainDropActive = false;
    std::shared_ptr<juce::FileChooser> terrainChooser;
    std::shared_ptr<juce::FileChooser> granularChooser;
    juce::ThreadPool sampleLoadPool { 1 };

    // Level + pitch controls shared by all osc types
    juce::Slider levelSlider, tuneSlider, octaveSlider;
    std::unique_ptr<Att> levelAtt, tuneAtt, octaveAtt;
    std::array<juce::Slider, 4> regionSliders;
    std::array<std::unique_ptr<Att>, 4> regionAtts;
    std::array<juce::Label, 4> regionLabels;
    juce::ToggleButton transientSlicesButton { "AUTO" };
    std::unique_ptr<BAtt> transientSlicesAtt;

    // ADSR sliders
    juce::Slider adsrA, adsrD, adsrS, adsrR;
    std::unique_ptr<Att> attA, attD, attS, attR;

    juce::ComboBox modeBoxA, modeBoxB, modeBoxC;
    std::unique_ptr<CAtt> modeAttA, modeAttB, modeAttC;

    // Waveform preview — only created for VA panels
    std::unique_ptr<VAWaveDisplay> waveDisplay;
    std::unique_ptr<Osc3DFieldDisplay> field3dDisplay;
    std::unique_ptr<GranularWaveformDisplay> granularWaveform;
    std::unique_ptr<TerrainHeightmapDisplay> terrainDisplay;

    juce::Slider extra1, extra2, extra3, extra4, extra5, extra6, extra7, extra8, extra9, extra10;
    juce::Slider extra11, extra12, extra13, extra14, extra15, extra16, extra17, extra18, extra19, extra20;
    std::unique_ptr<Att> extraAtt1, extraAtt2, extraAtt3, extraAtt4, extraAtt5,
                         extraAtt6, extraAtt7, extraAtt8, extraAtt9, extraAtt10,
                         extraAtt11, extraAtt12, extraAtt13, extraAtt14, extraAtt15,
                         extraAtt16, extraAtt17, extraAtt18, extraAtt19, extraAtt20;
    int numExtraSliders = 0;
    int numModeBoxes = 0;

    juce::Label levelLabel, tuneLabel, octaveLabel;
    juce::Label retrigLabel;
    juce::Label modeALabel, modeBLabel, modeCLabel;
    std::array<juce::Label, 20> extraLabels;
    std::array<juce::Label,  4> adsrLabels;  // A D S R

    void setupSlider(juce::Slider& slider, const juce::String& tooltip);
    void setupADSRSlider(juce::Slider& slider, const juce::String& tooltip);
    void setupExtraSlider(juce::Slider& slider, const juce::String& paramId,
                          const juce::String& tooltip, std::unique_ptr<Att>& attachment);
    void launchTerrainImportChooser();
    void importTerrainHeightmapFile(const juce::File& file);
    void refreshTerrainDisplay();
    void launchGranularImportChooser();
    void importGranularSampleFile(const juce::File& file);
};
