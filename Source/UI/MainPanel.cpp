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
    // FM operator controls are added in buildEngineTab
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
    masterLabel.setFont(juce::Font(13.f, juce::Font::bold));
    addAndMakeVisible(masterLabel);

    auto setupKnob = [&](juce::Slider& s, const juce::String& paramId,
                         const juce::String& tip, std::unique_ptr<Att>& att) {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 16);
        s.setTooltip(tip);
        addAndMakeVisible(s);
        att = std::make_unique<Att>(apvts, paramId, s);
    };

    setupKnob(masterVolSlider,  "master_volume", "Master Volume", masterVolAtt);
    setupKnob(masterTuneSlider, "master_tune",   "Master Tune",   masterTuneAtt);
    setupKnob(masterPanSlider,  "master_pan",    "Master Pan",    masterPanAtt);
    setupKnob(masterA,          "master_adsr_a", "Attack",        masterAAtt);
    setupKnob(masterD,          "master_adsr_d", "Decay",         masterDAtt);
    setupKnob(masterS,          "master_adsr_s", "Sustain",       masterSAtt);
    setupKnob(masterR,          "master_adsr_r", "Release",       masterRAtt);
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
