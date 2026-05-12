#pragma once
#include <JuceHeader.h>
#include "OscPanel.h"
#include "FilterPanel.h"
#include "ModMatrixPanel.h"
#include "ArpPanel.h"
#include "LFOPanel.h"
#include <array>

class MainPanel : public juce::Component
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
    std::unique_ptr<LFOPanel>        lfoPanel;

    // Master controls
    juce::Slider    masterVolSlider, masterTuneSlider, masterPanSlider;
    juce::Slider    masterA, masterD, masterS, masterR;
    juce::Label     masterLabel;
    std::array<juce::Label, 7> masterLabels;   // VOL TUNE PAN ATK DEC SUS REL

    // FM controls
    juce::ComboBox  fmAlgorithmBox;
    juce::Slider    fmLevelSlider, fmOctaveSlider;
    std::array<juce::Slider,   10> fmOpCoarseSliders;
    std::array<juce::Slider,   10> fmOpLevelSliders;
    std::array<juce::ComboBox, 10> fmOpWaveBoxes;
    std::array<juce::Slider,   10> fmOpVelSliders;
    std::array<juce::Slider,   10> fmOpKeySliders;
    juce::Label     fmAlgoLabel, fmLvlLabel, fmOctLabel;
    juce::Label     fmCoarseHdrLabel, fmLevelHdrLabel, fmWaveHdrLabel;
    juce::Label     fmVelHdrLabel, fmKeyHdrLabel;
    std::array<juce::Label, 10> fmOpLabels;    // "0"–"9" operator index

    using Att  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<Att> masterVolAtt, masterTuneAtt, masterPanAtt;
    std::unique_ptr<Att> masterAAtt, masterDAtt, masterSAtt, masterRAtt;
    std::unique_ptr<CAtt> fmAlgorithmAtt;
    std::unique_ptr<Att> fmLevelAtt, fmOctaveAtt;
    std::array<std::unique_ptr<Att>,  10> fmOpCoarseAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpLevelAtts;
    std::array<std::unique_ptr<CAtt>, 10> fmOpWaveAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpVelAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpKeyAtts;

    juce::Component* oscillatorsTab = nullptr;
    juce::Component* engineTab      = nullptr;
    juce::Component* effectsTab     = nullptr;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    void buildOscillatorsTab();
    void buildEngineTab();
    void buildEffectsTab();
    void setupKnob(juce::Slider& slider, const juce::String& paramId,
                   const juce::String& tooltip, std::unique_ptr<Att>& attachment,
                   juce::Component& parent);
};
