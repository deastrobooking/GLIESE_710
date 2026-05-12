#pragma once
#include <JuceHeader.h>
#include "OscPanel.h"
#include "FilterPanel.h"
#include "ModMatrixPanel.h"
#include "ArpPanel.h"
#include <array>

class MainPanel : public juce::TabbedComponent
{
public:
    explicit MainPanel(juce::AudioProcessorValueTreeState& apvts);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Oscillator sub-panels
    std::unique_ptr<OscPanel>        osc3dPanel;
    std::unique_ptr<OscPanel>        oscVAPanel;
    std::unique_ptr<OscPanel>        oscWTPanel;
    std::unique_ptr<FilterPanel>     filterPanel;
    std::unique_ptr<ModMatrixPanel>  modPanel;
    std::unique_ptr<ArpPanel>        arpPanel;

    // Master controls
    juce::Slider    masterVolSlider, masterTuneSlider, masterPanSlider;
    juce::Slider    masterA, masterD, masterS, masterR;
    juce::Label     masterLabel;

    // FM controls
    juce::ComboBox  fmAlgorithmBox;
    juce::Slider    fmLevelSlider, fmOctaveSlider;
    std::array<juce::Slider, 10> fmOpCoarseSliders;
    std::array<juce::Slider, 10> fmOpLevelSliders;

    using Att = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<Att> masterVolAtt, masterTuneAtt, masterPanAtt;
    std::unique_ptr<Att> masterAAtt, masterDAtt, masterSAtt, masterRAtt;
    std::unique_ptr<CAtt> fmAlgorithmAtt;
    std::unique_ptr<Att> fmLevelAtt, fmOctaveAtt;
    std::array<std::unique_ptr<Att>, 10> fmOpCoarseAtts;
    std::array<std::unique_ptr<Att>, 10> fmOpLevelAtts;

    juce::Component* oscillatorsTab;
    juce::Component* engineTab;
    juce::Component* effectsTab;

    void buildOscillatorsTab();
    void buildEngineTab();
    void buildEffectsTab();
    void setupKnob(juce::Slider& slider, const juce::String& paramId,
                   const juce::String& tooltip, std::unique_ptr<Att>& attachment,
                   juce::Component& parent);
};
