#include "MainPanel.h"

static juce::Colour kBg    { 0xff0d0d0d };
static juce::Colour kAccent{ 0xffcc2222 };

MainPanel::MainPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    tabs.setTabBarDepth(28);
    tabs.setOutline(0);
    tabs.setColour(juce::TabbedComponent::backgroundColourId, kBg);
    addAndMakeVisible(tabs);

    // ── Oscillators tab ───────────────────────────────────────────────────────
    oscillatorsTab = new juce::Component();

    osc3dPanel = std::make_unique<OscPanel>(apvts, "3d", "3D OSCILLATOR");
    oscVAPanel = std::make_unique<OscPanel>(apvts, "va", "VIRTUAL ANALOG");
    oscWTPanel = std::make_unique<OscPanel>(apvts, "wt", "WAVETABLE");

    oscillatorsTab->addAndMakeVisible(*osc3dPanel);
    oscillatorsTab->addAndMakeVisible(*oscVAPanel);
    oscillatorsTab->addAndMakeVisible(*oscWTPanel);

    tabs.addTab("OSCILLATORS", kBg, oscillatorsTab, true);

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

    static const juce::StringArray kOpWaves { "Sine","Triangle","Saw","Square","Pulse","Noise" };

    for (int i = 0; i < 10; ++i)
    {
        auto pfx = "op" + juce::String(i) + "_";
        setupKnob(fmOpCoarseSliders[(size_t)i], pfx + "coarse",
                  "Operator " + juce::String(i) + " Coarse Ratio",
                  fmOpCoarseAtts[(size_t)i], *engineTab);
        setupKnob(fmOpLevelSliders[(size_t)i], pfx + "level",
                  "Operator " + juce::String(i) + " Level",
                  fmOpLevelAtts[(size_t)i], *engineTab);

        fmOpWaveBoxes[(size_t)i].addItemList(kOpWaves, 1);
        fmOpWaveBoxes[(size_t)i].setTooltip("Operator " + juce::String(i) + " Waveform");
        engineTab->addAndMakeVisible(fmOpWaveBoxes[(size_t)i]);
        fmOpWaveAtts[(size_t)i] = std::make_unique<CAtt>(apvts, pfx + "wave", fmOpWaveBoxes[(size_t)i]);

        setupKnob(fmOpVelSliders[(size_t)i], pfx + "vel",
                  "Operator " + juce::String(i) + " Velocity Depth",
                  fmOpVelAtts[(size_t)i], *engineTab);
        setupKnob(fmOpKeySliders[(size_t)i], pfx + "key",
                  "Operator " + juce::String(i) + " Key Scale",
                  fmOpKeyAtts[(size_t)i], *engineTab);
    }
    tabs.addTab("FM ENGINE", kBg, engineTab, true);

    // ── Filter + Mod + Arp tab ────────────────────────────────────────────────
    effectsTab = new juce::Component();

    filterPanel = std::make_unique<FilterPanel>(apvts);
    modPanel    = std::make_unique<ModMatrixPanel>(apvts);
    arpPanel    = std::make_unique<ArpPanel>(apvts);
    lfoPanel    = std::make_unique<LFOPanel>(apvts);

    effectsTab->addAndMakeVisible(*filterPanel);
    effectsTab->addAndMakeVisible(*modPanel);
    effectsTab->addAndMakeVisible(*arpPanel);
    effectsTab->addAndMakeVisible(*lfoPanel);

    tabs.addTab("FILTER / MOD / ARP", kBg, effectsTab, true);

    // ── Master controls ───────────────────────────────────────────────────────
    masterLabel.setText("MASTER", juce::dontSendNotification);
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    masterLabel.setFont(juce::Font(juce::FontOptions { 13.f, juce::Font::bold }));
    addAndMakeVisible(masterLabel);

    setupKnob(masterVolSlider,  "master_volume", "Master Volume", masterVolAtt, *this);
    setupKnob(masterTuneSlider, "master_tune",   "Master Tune",   masterTuneAtt, *this);
    setupKnob(masterPanSlider,  "master_pan",    "Master Pan",    masterPanAtt, *this);
    setupKnob(masterA,          "master_adsr_a", "Attack",        masterAAtt, *this);
    setupKnob(masterD,          "master_adsr_d", "Decay",         masterDAtt, *this);
    setupKnob(masterS,          "master_adsr_s", "Sustain",       masterSAtt, *this);
    setupKnob(masterR,          "master_adsr_r", "Release",       masterRAtt, *this);

    // ── Label helpers ─────────────────────────────────────────────────────────
    auto makeSmallLabel = [&](juce::Label& l, const juce::String& text, juce::Component& parent) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colour(0xffcc2222));
        l.setFont(juce::Font(juce::FontOptions { 10.f }));
        l.setJustificationType(juce::Justification::centred);
        parent.addAndMakeVisible(l);
    };

    const juce::String masterLabelTexts[] = { "VOL","TUNE","PAN","ATK","DEC","SUS","REL" };
    for (int i = 0; i < 7; ++i)
        makeSmallLabel(masterLabels[(size_t)i], masterLabelTexts[i], *this);

    makeSmallLabel(fmAlgoLabel,      "ALGORITHM",    *engineTab);
    makeSmallLabel(fmLvlLabel,       "FM LEVEL",     *engineTab);
    makeSmallLabel(fmOctLabel,       "FM OCTAVE",    *engineTab);
    makeSmallLabel(fmCoarseHdrLabel, "COARSE RATIO", *engineTab);
    makeSmallLabel(fmLevelHdrLabel,  "OP LEVEL",     *engineTab);
    makeSmallLabel(fmWaveHdrLabel,   "WAVEFORM",     *engineTab);
    makeSmallLabel(fmVelHdrLabel,    "VEL DEPTH",    *engineTab);
    makeSmallLabel(fmKeyHdrLabel,    "KEY SCALE",    *engineTab);
    for (int i = 0; i < 10; ++i)
        makeSmallLabel(fmOpLabels[(size_t)i], juce::String(i), *engineTab);
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

    // Reserve bottom strip for master controls (extra 14px for label row)
    auto masterStrip = area.removeFromBottom(124);

    // MASTER title spans full strip height on the left
    masterLabel.setBounds(masterStrip.removeFromLeft(70));

    // Split remaining strip into label row (top 14px) + knob row
    auto masterLabelRow = masterStrip.removeFromTop(14);
    auto masterKnobRow  = masterStrip;

    auto positionMaster = [&](juce::Label& lbl, juce::Slider& knob, int w) {
        lbl .setBounds(masterLabelRow.removeFromLeft(w));
        knob.setBounds(masterKnobRow .removeFromLeft(w));
    };

    positionMaster(masterLabels[0], masterVolSlider,  70);
    positionMaster(masterLabels[1], masterTuneSlider, 70);
    positionMaster(masterLabels[2], masterPanSlider,  70);
    masterLabelRow.removeFromLeft(10);
    masterKnobRow .removeFromLeft(10);
    positionMaster(masterLabels[3], masterA, 70);
    positionMaster(masterLabels[4], masterD, 70);
    positionMaster(masterLabels[5], masterS, 70);
    positionMaster(masterLabels[6], masterR, 70);

    tabs.setBounds(area);

    // Layout oscillators tab content
    if (oscillatorsTab && osc3dPanel && oscVAPanel && oscWTPanel)
    {
        auto tabArea = oscillatorsTab->getLocalBounds().reduced(6);
        int w3 = tabArea.getWidth() / 3;
        osc3dPanel->setBounds(tabArea.removeFromLeft(w3).reduced(3));
        oscVAPanel->setBounds(tabArea.removeFromLeft(w3).reduced(3));
        oscWTPanel->setBounds(tabArea.reduced(3));
    }

    // Layout FM ENGINE tab content
    if (engineTab)
    {
        auto tabArea = engineTab->getLocalBounds().reduced(8);
        const int opW = juce::jmax(64, tabArea.getWidth() / 10);

        // Top row: algo + level + octave
        auto topLabelRow = tabArea.removeFromTop(13);
        fmAlgoLabel.setBounds(topLabelRow.removeFromLeft(180));
        fmLvlLabel .setBounds(topLabelRow.removeFromLeft(80));
        fmOctLabel .setBounds(topLabelRow.removeFromLeft(80));

        auto topRow = tabArea.removeFromTop(84);
        fmAlgorithmBox.setBounds(topRow.removeFromLeft(180).withHeight(24));
        fmLevelSlider .setBounds(topRow.removeFromLeft(80));
        fmOctaveSlider.setBounds(topRow.removeFromLeft(80));

        tabArea.removeFromTop(6);

        // Operator index labels (shared across all op rows)
        fmCoarseHdrLabel.setBounds(tabArea.removeFromTop(13));
        auto opIndexRow = tabArea.removeFromTop(13);
        for (int i = 0; i < 10; ++i)
            fmOpLabels[(size_t)i].setBounds(opIndexRow.removeFromLeft(opW));

        // Coarse ratio row
        auto coarseRow = tabArea.removeFromTop(72);
        for (int i = 0; i < 10; ++i)
            fmOpCoarseSliders[(size_t)i].setBounds(coarseRow.removeFromLeft(opW).reduced(2));

        tabArea.removeFromTop(4);

        // Level row
        fmLevelHdrLabel.setBounds(tabArea.removeFromTop(13));
        auto levelRow = tabArea.removeFromTop(72);
        for (int i = 0; i < 10; ++i)
            fmOpLevelSliders[(size_t)i].setBounds(levelRow.removeFromLeft(opW).reduced(2));

        tabArea.removeFromTop(4);

        // Waveform combo row
        fmWaveHdrLabel.setBounds(tabArea.removeFromTop(13));
        auto waveRow = tabArea.removeFromTop(22);
        for (int i = 0; i < 10; ++i)
            fmOpWaveBoxes[(size_t)i].setBounds(waveRow.removeFromLeft(opW).reduced(2, 0));

        tabArea.removeFromTop(4);

        // Velocity depth row
        fmVelHdrLabel.setBounds(tabArea.removeFromTop(13));
        auto velRow = tabArea.removeFromTop(juce::jmin(72, tabArea.getHeight() / 2));
        for (int i = 0; i < 10; ++i)
            fmOpVelSliders[(size_t)i].setBounds(velRow.removeFromLeft(opW).reduced(2));

        tabArea.removeFromTop(4);

        // Key scaling row
        fmKeyHdrLabel.setBounds(tabArea.removeFromTop(13));
        auto keyRow = tabArea;
        for (int i = 0; i < 10; ++i)
            fmOpKeySliders[(size_t)i].setBounds(keyRow.removeFromLeft(opW).reduced(2));
    }

    // Layout filter/mod/arp/lfo tab content
    if (effectsTab && filterPanel && modPanel && arpPanel && lfoPanel)
    {
        auto tabArea = effectsTab->getLocalBounds().reduced(6);
        int filterH = 220;
        filterPanel->setBounds(tabArea.removeFromTop(filterH).reduced(3));
        tabArea.removeFromTop(4);

        // Bottom row: mod matrix left, LFO centre-right, arp far-right
        int arpW = 240;
        int lfoW = 280;
        arpPanel->setBounds(tabArea.removeFromRight(arpW).reduced(3));
        lfoPanel->setBounds(tabArea.removeFromRight(lfoW).reduced(3));
        modPanel->setBounds(tabArea.reduced(3));
    }
}
