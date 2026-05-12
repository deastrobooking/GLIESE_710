#pragma once
#include <JuceHeader.h>
#include "OscPanel.h"
#include "FilterPanel.h"
#include "ModMatrixPanel.h"
#include "ArpPanel.h"

class MainPanel : public juce::Component,
                  public juce::TabbedComponent
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

    using Att = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Att> masterVolAtt, masterTuneAtt, masterPanAtt;
    std::unique_ptr<Att> masterAAtt, masterDAtt, masterSAtt, masterRAtt;

    juce::Component* oscillatorsTab;
    juce::Component* engineTab;
    juce::Component* effectsTab;

    void buildOscillatorsTab();
    void buildEngineTab();
    void buildEffectsTab();
};
