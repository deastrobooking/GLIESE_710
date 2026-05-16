#pragma once
#include <JuceHeader.h>
#include "OscPanel.h"
#include "FilterPanel.h"
#include "ModMatrixPanel.h"
#include "ArpPanel.h"
#include "LFOPanel.h"
#include "EngineSlotBar.h"
#include "IndustrialTheme.h"
#include <array>

// Simple styled placeholder shown in oscillator tab for FM engine blocks.
class FMBlockView : public juce::Component
{
public:
    int engineIndex = 0;   // 0 or 1

    void paint(juce::Graphics& g) override
    {
        auto accent = IndustrialTheme::accentFM();
        auto b = getLocalBounds().toFloat().reduced(4.f);
        g.setColour(accent.withAlpha(0.05f));
        g.fillRoundedRectangle(b, 6.f);
        g.setColour(accent.withAlpha(0.35f));
        g.drawRoundedRectangle(b, 6.f, 1.5f);

        auto centre = getLocalBounds().getCentreY();
        g.setColour(accent);
        g.setFont(juce::Font(juce::FontOptions { 15.f, juce::Font::bold }));
        g.drawText("FM ENGINE " + juce::String(engineIndex + 1),
                   getLocalBounds().withY(centre - 20).withHeight(22),
                   juce::Justification::centred);
        g.setColour(accent.withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions { 10.f }));
        g.drawText("Shared controls: FM Controls tab",
                   getLocalBounds().withY(centre + 6).withHeight(14),
                   juce::Justification::centred);
    }
};

class MainPanel : public juce::Component
{
public:
    explicit MainPanel(juce::AudioProcessorValueTreeState& apvts);
    ~MainPanel() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // ── Oscillator panels (all 8 VA + shared panels) ──────────────────────────
    std::unique_ptr<OscPanel>        osc3dPanel;
    std::unique_ptr<OscPanel>        oscWTPanel;
    std::unique_ptr<OscPanel>        ksPanel;
    std::unique_ptr<OscPanel>        granularPanel;
    std::array<std::unique_ptr<OscPanel>, 8> vaPanels;  // VA1–8
    std::array<FMBlockView, 2>       fmBlockViews;      // inline FM placeholders

    // ── Engine slot bar ───────────────────────────────────────────────────────
    std::unique_ptr<EngineSlotBar>   slotBar;
    std::array<juce::TextButton, 2>  oscPageButtons;
    int                              activeOscPage = 0;

    // ── Engine config knobs (detune params, always visible) ───────────────────
    juce::Slider    eng3dDetuneSlider, engFMDetuneSlider;
    std::array<juce::Label, 2> engDetuneLabels;

    // ── Filter / Mod / Arp panels ─────────────────────────────────────────────
    std::unique_ptr<FilterPanel>     filterPanel;
    std::unique_ptr<ModMatrixPanel>  modPanel;
    std::unique_ptr<ArpPanel>        arpPanel;
    std::unique_ptr<LFOPanel>        lfoPanel;

    // ── Master controls ───────────────────────────────────────────────────────
    juce::Slider    masterVolSlider, masterTuneSlider, masterOctaveSlider, masterPanSlider;
    juce::Slider    masterPortaSlider, masterAgeSlider, masterFeedSlider;
    juce::Slider    masterPBSlider, masterMWSlider;
    juce::Slider    masterA, masterD, masterS, masterR;
    juce::Label     masterLabel;
    std::array<juce::Label, 13> masterLabels;

    // ── FM engine controls (Tab 2) ────────────────────────────────────────────
    juce::ComboBox  fmAlgorithmBox;
    juce::Slider    fmLevelSlider, fmOctaveSlider;
    juce::Slider    fmA, fmD, fmS, fmR;
    std::array<juce::Slider,   10> fmOpCoarseSliders;
    std::array<juce::Slider,   10> fmOpFineSliders;
    std::array<juce::Slider,   10> fmOpDetuneSliders;
    std::array<juce::Slider,   10> fmOpLevelSliders;
    std::array<juce::Slider,   10> fmOpFeedbackSliders;
    std::array<juce::ComboBox, 10> fmOpWaveBoxes;
    std::array<juce::Slider,   10> fmOpSkirtSliders;
    std::array<juce::Slider,   10> fmOpResSliders;
    std::array<juce::Slider,   10> fmOpVelSliders;
    std::array<juce::Slider,   10> fmOpKeySliders;
    juce::Label     fmAlgoLabel, fmLvlLabel, fmOctLabel;
    std::array<juce::Label, 4>  fmAdsrLabels;
    juce::Label     fmCoarseHdrLabel, fmFineHdrLabel, fmDetuneHdrLabel;
    juce::Label     fmLevelHdrLabel, fmFeedbackHdrLabel, fmWaveHdrLabel;
    juce::Label     fmSkirtHdrLabel, fmResHdrLabel, fmVelHdrLabel, fmKeyHdrLabel;
    std::array<juce::Label, 10> fmOpLabels;
    juce::ToggleButton fmEnableButton;
    juce::Label        fmEnableLabel;

    using Att  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<Att>  masterVolAtt, masterTuneAtt, masterOctaveAtt, masterPanAtt;
    std::unique_ptr<Att>  masterPortaAtt, masterAgeAtt, masterFeedAtt;
    std::unique_ptr<Att>  masterPBAtt, masterMWAtt;
    std::unique_ptr<Att>  masterAAtt, masterDAtt, masterSAtt, masterRAtt;
    std::unique_ptr<CAtt> fmAlgorithmAtt;
    std::unique_ptr<Att>  fmLevelAtt, fmOctaveAtt;
    std::unique_ptr<Att>  fmAAtt, fmDAtt, fmSAtt, fmRAtt;
    std::unique_ptr<BAtt> fmEnableAtt;
    std::array<std::unique_ptr<Att>,  10> fmOpCoarseAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpFineAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpDetuneAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpLevelAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpFeedbackAtts;
    std::array<std::unique_ptr<CAtt>, 10> fmOpWaveAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpSkirtAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpResAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpVelAtts;
    std::array<std::unique_ptr<Att>,  10> fmOpKeyAtts;
    std::unique_ptr<Att>  eng3dDetuneAtt, engFMDetuneAtt;

    juce::Component* oscillatorsTab = nullptr;
    juce::Component* engineTab      = nullptr;
    juce::Component* effectsTab     = nullptr;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    IndustrialTheme::IndustrialLookAndFeel lookAndFeel;

    void buildOscillatorsTab();
    void buildEngineTab();
    void buildEffectsTab();
    void layoutOscillatorsTab();  // called on resize + slot changes
    void setActiveOscPage(int page);
    void applyLookAndFeel(juce::Component&);
    void clearLookAndFeel(juce::Component&);
    void setupKnob(juce::Slider&, const juce::String& paramId,
                   const juce::String& tooltip, std::unique_ptr<Att>&,
                   juce::Component& parent,
                   juce::Colour accent = IndustrialTheme::accentMaster());
};
