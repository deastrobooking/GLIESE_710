#include "MainPanel.h"

static juce::Colour kBg    { 0xff0d0d14 };
static juce::Colour kAccent{ 0xff4466cc };

MainPanel::MainPanel(juce::AudioProcessorValueTreeState& a)
    : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop),
      apvts(a)
{
    setTabBarDepth(28);
    setOutline(0);
    setColour(juce::TabbedComponent::backgroundColourId, kBg);

    // ── Oscillators tab ───────────────────────────────────────────────────────
    oscillatorsTab = new juce::Component();

    osc3dPanel = std::make_unique<OscPanel>(apvts, "3d", "3D OSCILLATOR");
    oscVAPanel = std::make_unique<OscPanel>(apvts, "va", "VIRTUAL ANALOG");
    oscWTPanel = std::make_unique<OscPanel>(apvts, "wt", "WAVETABLE");

    oscillatorsTab->addAndMakeVisible(*osc3dPanel);
    oscillatorsTab->addAndMakeVisible(*oscVAPanel);
    oscillatorsTab->addAndMakeVisible(*oscWTPanel);

    addTab("OSCILLATORS", kBg, oscillatorsTab, true);

    // ── FM Engine tab ─────────────────────────────────────────────────────────
    engineTab = new juce::Component();
    fmAlgorithmBox.addItemList([]
    {
        juce::StringArray items;
        for (int i = 0; i < 40; ++i)
            items.add("Algorithm " + juce::String(i));
        return items;
    }(), 1);
    fmAlgorithmBox.setTooltip("FM Algorithm");
    engineTab->addAndMakeVisible(fmAlgorithmBox);
    fmAlgorithmAtt = std::make_unique<CAtt>(apvts, "osc_fm_algorithm", fmAlgorithmBox);

    setupKnob(fmLevelSlider, "osc_fm_level", "FM Level", fmLevelAtt, *engineTab);
    setupKnob(fmOctaveSlider, "osc_fm_octave", "FM Octave", fmOctaveAtt, *engineTab);

    for (int i = 0; i < 10; ++i)
    {
        auto pfx = "op" + juce::String(i) + "_";
        setupKnob(fmOpCoarseSliders[(size_t)i], pfx + "coarse",
                  "Operator " + juce::String(i) + " Coarse Ratio",
                  fmOpCoarseAtts[(size_t)i], *engineTab);
        setupKnob(fmOpLevelSliders[(size_t)i], pfx + "level",
                  "Operator " + juce::String(i) + " Level",
                  fmOpLevelAtts[(size_t)i], *engineTab);
    }
    addTab("FM ENGINE", kBg, engineTab, true);

    // ── Filter + Mod + Arp tab ────────────────────────────────────────────────
    effectsTab = new juce::Component();

    filterPanel = std::make_unique<FilterPanel>(apvts);
    modPanel    = std::make_unique<ModMatrixPanel>(apvts);
    arpPanel    = std::make_unique<ArpPanel>(apvts);

    effectsTab->addAndMakeVisible(*filterPanel);
    effectsTab->addAndMakeVisible(*modPanel);
    effectsTab->addAndMakeVisible(*arpPanel);

    addTab("FILTER / MOD / ARP", kBg, effectsTab, true);

    // ── Master controls ───────────────────────────────────────────────────────
    masterLabel.setText("MASTER", juce::dontSendNotification);
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::lightcyan);
    masterLabel.setFont(juce::Font(juce::FontOptions { 13.f, juce::Font::bold }));
    addAndMakeVisible(masterLabel);

    setupKnob(masterVolSlider,  "master_volume", "Master Volume", masterVolAtt, *this);
    setupKnob(masterTuneSlider, "master_tune",   "Master Tune",   masterTuneAtt, *this);
    setupKnob(masterPanSlider,  "master_pan",    "Master Pan",    masterPanAtt, *this);
    setupKnob(masterA,          "master_adsr_a", "Attack",        masterAAtt, *this);
    setupKnob(masterD,          "master_adsr_d", "Decay",         masterDAtt, *this);
    setupKnob(masterS,          "master_adsr_s", "Sustain",       masterSAtt, *this);
    setupKnob(masterR,          "master_adsr_r", "Release",       masterRAtt, *this);
}

void MainPanel::setupKnob(juce::Slider& s, const juce::String& paramId,
                          const juce::String& tip, std::unique_ptr<Att>& att,
                          juce::Component& parent)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 16);
    s.setTooltip(tip);
    parent.addAndMakeVisible(s);
    att = std::make_unique<Att>(apvts, paramId, s);
}

void MainPanel::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
}

void MainPanel::resized()
{
    auto area = getLocalBounds();

    // Reserve bottom strip for master controls
    auto masterStrip = area.removeFromBottom(110);

    // Layout master controls
    masterLabel.setBounds(masterStrip.removeFromLeft(70));
    int kw = 70;
    masterVolSlider .setBounds(masterStrip.removeFromLeft(kw));
    masterTuneSlider.setBounds(masterStrip.removeFromLeft(kw));
    masterPanSlider .setBounds(masterStrip.removeFromLeft(kw));
    masterStrip.removeFromLeft(10);
    masterA.setBounds(masterStrip.removeFromLeft(kw));
    masterD.setBounds(masterStrip.removeFromLeft(kw));
    masterS.setBounds(masterStrip.removeFromLeft(kw));
    masterR.setBounds(masterStrip.removeFromLeft(kw));

    // TabbedComponent takes the rest
    juce::TabbedComponent::setBounds(area);

    // Layout oscillators tab content
    if (oscillatorsTab)
    {
        auto tabArea = oscillatorsTab->getLocalBounds().reduced(6);
        int w3 = tabArea.getWidth() / 3;
        osc3dPanel->setBounds(tabArea.removeFromLeft(w3).reduced(3));
        oscVAPanel->setBounds(tabArea.removeFromLeft(w3).reduced(3));
        oscWTPanel->setBounds(tabArea.reduced(3));
    }

    // Layout effects tab content
    if (engineTab)
    {
        auto tabArea = engineTab->getLocalBounds().reduced(8);
        auto topRow = tabArea.removeFromTop(84);
        fmAlgorithmBox.setBounds(topRow.removeFromLeft(180).withHeight(24));
        fmLevelSlider.setBounds(topRow.removeFromLeft(80));
        fmOctaveSlider.setBounds(topRow.removeFromLeft(80));

        tabArea.removeFromTop(8);
        const int opW = juce::jmax(70, tabArea.getWidth() / 10);
        auto coarseRow = tabArea.removeFromTop(86);
        for (int i = 0; i < 10; ++i)
            fmOpCoarseSliders[(size_t)i].setBounds(coarseRow.removeFromLeft(opW).reduced(2));

        auto levelRow = tabArea.removeFromTop(86);
        for (int i = 0; i < 10; ++i)
            fmOpLevelSliders[(size_t)i].setBounds(levelRow.removeFromLeft(opW).reduced(2));
    }

    // Layout effects tab content
    if (effectsTab)
    {
        auto tabArea = effectsTab->getLocalBounds().reduced(6);
        int filterH = 220;
        filterPanel->setBounds(tabArea.removeFromTop(filterH).reduced(3));
        tabArea.removeFromTop(4);
        int arpW = 260;
        arpPanel->setBounds(tabArea.removeFromRight(arpW).reduced(3));
        modPanel->setBounds(tabArea.reduced(3));
    }
}
