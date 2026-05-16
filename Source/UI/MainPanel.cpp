#include "MainPanel.h"
#include <vector>

static juce::Colour kBg { 0xff050607 };

//──────────────────────────────────────────────────────────────────────────────
MainPanel::MainPanel(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    tabs.setTabBarDepth(28);
    tabs.setOutline(0);
    tabs.setColour(juce::TabbedComponent::backgroundColourId,  kBg);
    tabs.setColour(juce::TabbedComponent::outlineColourId,     IndustrialTheme::panelEdge());
    tabs.setColour(juce::TabbedButtonBar::tabTextColourId,     IndustrialTheme::mutedText());
    tabs.setColour(juce::TabbedButtonBar::frontTextColourId,   IndustrialTheme::text());
    tabs.setColour(juce::TabbedButtonBar::tabOutlineColourId,  IndustrialTheme::panelEdge());
    tabs.setColour(juce::TabbedButtonBar::frontOutlineColourId,
                   IndustrialTheme::accentMaster().withAlpha(0.7f));
    addAndMakeVisible(tabs);

    buildOscillatorsTab();
    buildEngineTab();
    buildEffectsTab();

    // ── Master controls ───────────────────────────────────────────────────────
    masterLabel.setText("MASTER", juce::dontSendNotification);
    IndustrialTheme::styleTitle(masterLabel, IndustrialTheme::accentMaster(), 13.f);
    addAndMakeVisible(masterLabel);

    setupKnob(masterVolSlider,    "master_volume",    "Master Volume",    masterVolAtt,    *this);
    setupKnob(masterTuneSlider,   "master_tune",      "Master Tune",      masterTuneAtt,   *this);
    setupKnob(masterOctaveSlider, "master_octave",    "Master Octave",    masterOctaveAtt, *this);
    setupKnob(masterPanSlider,    "master_pan",       "Master Pan",       masterPanAtt,    *this);
    setupKnob(masterPortaSlider,  "master_portamento","Portamento Time",  masterPortaAtt,  *this);
    setupKnob(masterAgeSlider,    "master_age",       "Age / Bitcrusher", masterAgeAtt,    *this);
    setupKnob(masterFeedSlider,   "master_feedback",  "Output Feedback",  masterFeedAtt,   *this);
    setupKnob(masterPBSlider,     "master_pb_range",  "Pitch Bend Range", masterPBAtt,     *this);
    setupKnob(masterMWSlider,     "master_mw_range",  "Mod Wheel Range",  masterMWAtt,     *this);
    setupKnob(masterA,            "master_adsr_a",    "Attack",           masterAAtt,      *this);
    setupKnob(masterD,            "master_adsr_d",    "Decay",            masterDAtt,      *this);
    setupKnob(masterS,            "master_adsr_s",    "Sustain",          masterSAtt,      *this);
    setupKnob(masterR,            "master_adsr_r",    "Release",          masterRAtt,      *this);

    auto makeSmallLabel = [&](juce::Label& l, const juce::String& text, juce::Component& parent)
    {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, IndustrialTheme::accentMaster(), 10.f);
        parent.addAndMakeVisible(l);
    };
    const juce::String masterLabelTexts[] = {
        "VOL","TUNE","OCT","PAN","PORT","AGE","FEED","PB","MW","ATK","DEC","SUS","REL" };
    for (int i = 0; i < 13; ++i)
        makeSmallLabel(masterLabels[(size_t)i], masterLabelTexts[i], *this);

    applyLookAndFeel(*this);
}

MainPanel::~MainPanel()
{
    clearLookAndFeel(*this);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::buildOscillatorsTab()
{
    oscillatorsTab = new juce::Component();

    osc3dPanel = std::make_unique<OscPanel>(apvts, "3d", "3D OSCILLATOR");
    oscWTPanel = std::make_unique<OscPanel>(apvts, "wt", "WAVETABLE");
    ksPanel    = std::make_unique<OscPanel>(apvts, "ks", "KARPLUS-STRONG");
    granularPanel = std::make_unique<OscPanel>(apvts, "grain", "GRANULAR");

    static const juce::StringArray vaIds {
        "va","va2","va3","va4","va5","va6","va7","va8" };
    for (int i = 0; i < 8; ++i)
    {
        vaPanels[(size_t)i] = std::make_unique<OscPanel>(
            apvts, vaIds[i], "VIRTUAL ANALOG " + juce::String(i + 1));
        oscillatorsTab->addAndMakeVisible(*vaPanels[(size_t)i]);
    }

    for (int i = 0; i < 2; ++i)
    {
        fmBlockViews[(size_t)i].engineIndex = i;
        oscillatorsTab->addAndMakeVisible(fmBlockViews[(size_t)i]);
    }

    oscillatorsTab->addAndMakeVisible(*osc3dPanel);
    oscillatorsTab->addAndMakeVisible(*oscWTPanel);
    oscillatorsTab->addAndMakeVisible(*ksPanel);
    oscillatorsTab->addAndMakeVisible(*granularPanel);

    // Engine slot bar — drives the layout
    slotBar = std::make_unique<EngineSlotBar>(apvts, [this] { layoutOscillatorsTab(); });
    oscillatorsTab->addAndMakeVisible(*slotBar);

    for (int i = 0; i < 2; ++i)
    {
        auto& b = oscPageButtons[(size_t)i];
        b.setButtonText("PAGE " + juce::String(i + 1));
        b.setTooltip("Show engine page " + juce::String(i + 1));
        b.setClickingTogglesState(false);
        b.onClick = [this, i] { setActiveOscPage(i); };
        b.setColour(juce::TextButton::buttonColourId,    juce::Colour(0xff15181d));
        b.setColour(juce::TextButton::buttonOnColourId,  IndustrialTheme::accentMaster().withAlpha(0.32f));
        b.setColour(juce::TextButton::textColourOffId,   IndustrialTheme::mutedText());
        b.setColour(juce::TextButton::textColourOnId,    IndustrialTheme::text());
        oscillatorsTab->addAndMakeVisible(b);
    }

    // Detune knobs (always shown — relevant whenever >1 3D or dual FM)
    auto makeEngLabel = [&](juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, IndustrialTheme::accentMaster(), 9.f);
        oscillatorsTab->addAndMakeVisible(l);
    };
    makeEngLabel(engDetuneLabels[0], "3D DTN");
    makeEngLabel(engDetuneLabels[1], "FM DTN");

    setupKnob(eng3dDetuneSlider, "engine_3d_detune", "3D Ensemble Detune",
              eng3dDetuneAtt, *oscillatorsTab, IndustrialTheme::accentMaster());
    setupKnob(engFMDetuneSlider, "engine_fm_detune", "FM Dual Engine Detune (cents)",
              engFMDetuneAtt, *oscillatorsTab, IndustrialTheme::accentMaster());

    tabs.addTab("OSCILLATORS", kBg, oscillatorsTab, true);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::buildEngineTab()
{
    engineTab = new juce::Component();

    fmEnableButton.setButtonText("ON");
    fmEnableButton.setTooltip("Enable / disable the FM engine");
    fmEnableButton.setColour(juce::ToggleButton::textColourId,        IndustrialTheme::accentFM());
    fmEnableButton.setColour(juce::ToggleButton::tickColourId,        IndustrialTheme::accentFM());
    fmEnableButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff555555));
    engineTab->addAndMakeVisible(fmEnableButton);
    fmEnableAtt = std::make_unique<BAtt>(apvts, "osc_fm_on", fmEnableButton);

    fmEnableLabel.setText("FM CONTROLS", juce::dontSendNotification);
    IndustrialTheme::styleTitle(fmEnableLabel, IndustrialTheme::accentFM(), 13.f);
    engineTab->addAndMakeVisible(fmEnableLabel);

    fmAlgorithmBox.addItemList([]
    {
        juce::StringArray items;
        for (int i = 0; i < 40; ++i)
            items.add("Algorithm " + juce::String(i));
        return items;
    }(), 1);
    fmAlgorithmBox.setTooltip("FM Algorithm");
    IndustrialTheme::styleCombo(fmAlgorithmBox, IndustrialTheme::accentFM());
    engineTab->addAndMakeVisible(fmAlgorithmBox);
    fmAlgorithmAtt = std::make_unique<CAtt>(apvts, "osc_fm_algorithm", fmAlgorithmBox);

    setupKnob(fmLevelSlider, "osc_fm_level",  "FM Level",   fmLevelAtt,  *engineTab, IndustrialTheme::accentFM());
    setupKnob(fmOctaveSlider,"osc_fm_octave", "FM Octave",  fmOctaveAtt, *engineTab, IndustrialTheme::accentFM());
    setupKnob(fmA, "osc_fm_adsr_a", "FM Attack",  fmAAtt, *engineTab, IndustrialTheme::accentFM());
    setupKnob(fmD, "osc_fm_adsr_d", "FM Decay",   fmDAtt, *engineTab, IndustrialTheme::accentFM());
    setupKnob(fmS, "osc_fm_adsr_s", "FM Sustain", fmSAtt, *engineTab, IndustrialTheme::accentFM());
    setupKnob(fmR, "osc_fm_adsr_r", "FM Release", fmRAtt, *engineTab, IndustrialTheme::accentFM());
    for (auto* s : { &fmA, &fmD, &fmS, &fmR })
    {
        s->setSliderStyle(juce::Slider::LinearVertical);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 42, 13);
    }

    static const juce::StringArray kOpWaves {
        "Sine","Triangle","Saw","Square","Pulse","Noise",
        "All Wide","All Narrow","Odd Wide","Odd Narrow","Res Wide","Res Narrow"
    };

    auto compactOpSlider = [](juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 12);
    };

    for (int i = 0; i < 10; ++i)
    {
        auto pfx = "op" + juce::String(i) + "_";
        setupKnob(fmOpCoarseSliders[(size_t)i], pfx + "coarse",
                  "Op " + juce::String(i) + " Coarse", fmOpCoarseAtts[(size_t)i],
                  *engineTab, IndustrialTheme::accentFM());
        compactOpSlider(fmOpCoarseSliders[(size_t)i]);
        setupKnob(fmOpFineSliders[(size_t)i], pfx + "fine",
                  "Op " + juce::String(i) + " Fine Tune", fmOpFineAtts[(size_t)i],
                  *engineTab, IndustrialTheme::accentFM());
        compactOpSlider(fmOpFineSliders[(size_t)i]);
        setupKnob(fmOpDetuneSliders[(size_t)i], pfx + "detune",
                  "Op " + juce::String(i) + " Detune", fmOpDetuneAtts[(size_t)i],
                  *engineTab, IndustrialTheme::accentFM());
        compactOpSlider(fmOpDetuneSliders[(size_t)i]);
        setupKnob(fmOpLevelSliders[(size_t)i],  pfx + "level",
                  "Op " + juce::String(i) + " Level",  fmOpLevelAtts[(size_t)i],
                  *engineTab, IndustrialTheme::accentFM());
        compactOpSlider(fmOpLevelSliders[(size_t)i]);
        setupKnob(fmOpFeedbackSliders[(size_t)i], pfx + "feedback",
                  "Op " + juce::String(i) + " Feedback", fmOpFeedbackAtts[(size_t)i],
                  *engineTab, IndustrialTheme::accentFM());
        compactOpSlider(fmOpFeedbackSliders[(size_t)i]);

        fmOpWaveBoxes[(size_t)i].addItemList(kOpWaves, 1);
        fmOpWaveBoxes[(size_t)i].setTooltip("Op " + juce::String(i) + " Wave");
        IndustrialTheme::styleCombo(fmOpWaveBoxes[(size_t)i], IndustrialTheme::accentFM());
        engineTab->addAndMakeVisible(fmOpWaveBoxes[(size_t)i]);
        fmOpWaveAtts[(size_t)i] = std::make_unique<CAtt>(apvts, pfx + "wave", fmOpWaveBoxes[(size_t)i]);

        setupKnob(fmOpSkirtSliders[(size_t)i], pfx + "skirt",
                  "Op " + juce::String(i) + " Spectral Skirt", fmOpSkirtAtts[(size_t)i],
                  *engineTab, IndustrialTheme::accentFM());
        compactOpSlider(fmOpSkirtSliders[(size_t)i]);
        setupKnob(fmOpResSliders[(size_t)i], pfx + "resonance",
                  "Op " + juce::String(i) + " Spectral Resonance", fmOpResAtts[(size_t)i],
                  *engineTab, IndustrialTheme::accentFM());
        compactOpSlider(fmOpResSliders[(size_t)i]);
        setupKnob(fmOpVelSliders[(size_t)i], pfx + "vel",
                  "Op " + juce::String(i) + " Velocity", fmOpVelAtts[(size_t)i],
                  *engineTab, IndustrialTheme::accentFM());
        compactOpSlider(fmOpVelSliders[(size_t)i]);
        setupKnob(fmOpKeySliders[(size_t)i], pfx + "key",
                  "Op " + juce::String(i) + " Key Scale", fmOpKeyAtts[(size_t)i],
                  *engineTab, IndustrialTheme::accentFM());
        compactOpSlider(fmOpKeySliders[(size_t)i]);
    }

    auto makeFMLabel = [&](juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, IndustrialTheme::accentFM(), 10.f);
        engineTab->addAndMakeVisible(l);
    };
    makeFMLabel(fmAlgoLabel,      "ALGORITHM");
    makeFMLabel(fmLvlLabel,       "FM LEVEL");
    makeFMLabel(fmOctLabel,       "FM OCTAVE");
    const juce::String fmAdsrNames[] = { "ATK","DEC","SUS","REL" };
    for (int i = 0; i < 4; ++i)
        makeFMLabel(fmAdsrLabels[(size_t)i], fmAdsrNames[i]);
    makeFMLabel(fmCoarseHdrLabel, "COARSE RATIO");
    makeFMLabel(fmFineHdrLabel,   "FINE");
    makeFMLabel(fmDetuneHdrLabel, "DETUNE");
    makeFMLabel(fmLevelHdrLabel,  "OP LEVEL");
    makeFMLabel(fmFeedbackHdrLabel, "FEEDBACK");
    makeFMLabel(fmWaveHdrLabel,   "WAVEFORM");
    makeFMLabel(fmSkirtHdrLabel,  "SPECTRAL SKIRT");
    makeFMLabel(fmResHdrLabel,    "SPECTRAL RES");
    makeFMLabel(fmVelHdrLabel,    "VEL DEPTH");
    makeFMLabel(fmKeyHdrLabel,    "KEY SCALE");
    for (int i = 0; i < 10; ++i)
        makeFMLabel(fmOpLabels[(size_t)i], juce::String(i));

    tabs.addTab("FM CONTROLS", kBg, engineTab, true);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::buildEffectsTab()
{
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
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::applyLookAndFeel(juce::Component& c)
{
    c.setLookAndFeel(&lookAndFeel);
    for (int i = 0; i < c.getNumChildComponents(); ++i)
        applyLookAndFeel(*c.getChildComponent(i));
}

void MainPanel::clearLookAndFeel(juce::Component& c)
{
    c.setLookAndFeel(nullptr);
    for (int i = 0; i < c.getNumChildComponents(); ++i)
        clearLookAndFeel(*c.getChildComponent(i));
}

void MainPanel::setupKnob(juce::Slider& s, const juce::String& paramId,
                          const juce::String& tip, std::unique_ptr<Att>& att,
                          juce::Component& parent, juce::Colour accent)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 16);
    s.setTooltip(tip);
    IndustrialTheme::styleSlider(s, accent);
    parent.addAndMakeVisible(s);
    att = std::make_unique<Att>(apvts, paramId, s);
}

void MainPanel::setActiveOscPage(int page)
{
    activeOscPage = juce::jlimit(0, 1, page);
    for (int i = 0; i < 2; ++i)
        oscPageButtons[(size_t)i].setToggleState(i == activeOscPage, juce::dontSendNotification);

    layoutOscillatorsTab();
    if (oscillatorsTab) oscillatorsTab->repaint();
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintFaceplate(g, getLocalBounds());
}

//──────────────────────────────────────────────────────────────────────────────
// Dynamically lays out the oscillator panels based on the current slot config.
// Called both from resized() and whenever the slot bar changes.
void MainPanel::layoutOscillatorsTab()
{
    if (!oscillatorsTab || !slotBar) return;

    auto tabArea = oscillatorsTab->getLocalBounds().reduced(6);

    // Slot bar — top strip
    const int kSlotBarH  = 34;
    slotBar->setBounds(tabArea.removeFromTop(kSlotBarH));
    tabArea.removeFromTop(4);

    // Page selector: each page maps to exactly one 4-slot engine block.
    auto pageRow = tabArea.removeFromTop(26);
    const int pageButtonW = juce::jmin(118, pageRow.getWidth() / 2);
    for (int i = 0; i < 2; ++i)
    {
        oscPageButtons[(size_t)i].setToggleState(i == activeOscPage, juce::dontSendNotification);
        oscPageButtons[(size_t)i].setBounds(pageRow.removeFromLeft(pageButtonW).reduced(2));
    }
    tabArea.removeFromTop(4);

    // Detune knobs — small strip below slot bar
    const int kDetuneH = 72;
    auto detuneStrip = tabArea.removeFromTop(kDetuneH);
    {
        auto labelRow = detuneStrip.removeFromTop(12);
        engDetuneLabels[0].setBounds(labelRow.removeFromLeft(70));
        labelRow.removeFromLeft(4);
        engDetuneLabels[1].setBounds(labelRow.removeFromLeft(70));
        eng3dDetuneSlider.setBounds(detuneStrip.removeFromLeft(70));
        detuneStrip.removeFromLeft(4);
        engFMDetuneSlider.setBounds(detuneStrip.removeFromLeft(70));
    }
    tabArea.removeFromTop(4);

    // Hide all engine panels first
    osc3dPanel->setVisible(false);
    oscWTPanel->setVisible(false);
    ksPanel->setVisible(false);
    granularPanel->setVisible(false);
    for (auto& p : vaPanels) if (p) p->setVisible(false);
    for (auto& v : fmBlockViews) v.setVisible(false);

    // Build a list of {component, unitWidth} for the active page.
    // Each "unit" = one of the current page's four slot columns.
    struct PanelSlot { juce::Component* comp; int units; };
    std::vector<PanelSlot> panels;

    int vaIndex = 0; // globally tracks which VA param set the next VA slot uses

    auto addOrGrow = [&](juce::Component* comp, int units = 1)
    {
        for (auto& p : panels)
        {
            if (p.comp == comp)
            {
                p.units += units;
                return;
            }
        }
        panels.push_back({comp, units});
    };

    auto addGap = [&] { panels.push_back({nullptr, 1}); };

    for (int b = 0; b < 2; ++b)
    {
        const bool isActivePage = b == activeOscPage;

        if (slotBar->getBlockMode(b) == 1) // FM block
        {
            if (isActivePage)
            {
                fmBlockViews[(size_t)b].setVisible(true);
                addOrGrow(&fmBlockViews[(size_t)b], 4);
            }
        }
        else // Multi block — scan 4 slots
        {
            for (int s = 0; s < 4; ++s)
            {
                int type = slotBar->getSlotType(b, s);
                switch (type)
                {
                    case 1: // 3D
                        if (isActivePage)
                        {
                            osc3dPanel->setVisible(true);
                            addOrGrow(osc3dPanel.get());
                        }
                        break;
                    case 2: // VA
                        if (isActivePage && vaIndex < 8 && vaPanels[(size_t)vaIndex])
                        {
                            vaPanels[(size_t)vaIndex]->setVisible(true);
                            panels.push_back({vaPanels[(size_t)vaIndex].get(), 1});
                        }
                        if (isActivePage && vaIndex >= 8) addGap();
                        ++vaIndex;
                        break;
                    case 3: // WT
                        if (isActivePage)
                        {
                            oscWTPanel->setVisible(true);
                            addOrGrow(oscWTPanel.get());
                        }
                        break;
                    case 4: // KS
                        if (isActivePage)
                        {
                            ksPanel->setVisible(true);
                            addOrGrow(ksPanel.get());
                        }
                        break;
                    case 5: // Granular
                        if (isActivePage)
                        {
                            granularPanel->setVisible(true);
                            addOrGrow(granularPanel.get());
                        }
                        break;
                    default:
                        if (isActivePage) addGap();
                        break;
                }
            }
        }
    }

    auto layoutWide = [&]
    {
        const float unitW = (float)tabArea.getWidth() / 4.f;
        float x = (float)tabArea.getX();
        for (auto& p : panels)
        {
            int w = juce::roundToInt((float)p.units * unitW);
            if (p.comp != nullptr)
                p.comp->setBounds(juce::roundToInt(x), tabArea.getY(), w, tabArea.getHeight());
            x += (float)p.units * unitW;
        }
    };

    auto layoutCompact = [&]
    {
        const int gap = 4;
        const int columns = 2;
        const int cellW = (tabArea.getWidth() - gap) / columns;
        const int rowH  = (tabArea.getHeight() - gap) / 2;
        int row = 0;
        int col = 0;

        auto advance = [&](int span)
        {
            col += span;
            if (col >= columns)
            {
                ++row;
                col = 0;
            }
        };

        for (auto& p : panels)
        {
            int span = juce::jlimit(1, columns, p.units);
            if (col + span > columns)
            {
                ++row;
                col = 0;
            }

            if (p.comp != nullptr)
            {
                auto r = juce::Rectangle<int>(
                    tabArea.getX() + col * (cellW + gap),
                    tabArea.getY() + row * (rowH + gap),
                    cellW * span + gap * (span - 1),
                    rowH);
                p.comp->setBounds(r);
            }

            advance(span);
        }
    };

    const bool compact = tabArea.getWidth() < 980 && panels.size() > 2
                         && slotBar->getBlockMode(activeOscPage) == 0;
    compact ? layoutCompact() : layoutWide();
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::resized()
{
    auto area = getLocalBounds();

    // Master strip at bottom
    auto masterStrip = area.removeFromBottom(124);
    masterLabel.setBounds(masterStrip.removeFromLeft(70));

    auto masterLabelRow = masterStrip.removeFromTop(14);
    auto masterKnobRow  = masterStrip;

    auto positionMaster = [&](juce::Label& lbl, juce::Slider& knob, int w)
    {
        lbl .setBounds(masterLabelRow.removeFromLeft(w));
        knob.setBounds(masterKnobRow .removeFromLeft(w));
    };

    positionMaster(masterLabels[0], masterVolSlider,    60);
    positionMaster(masterLabels[1], masterTuneSlider,   60);
    positionMaster(masterLabels[2], masterOctaveSlider, 60);
    positionMaster(masterLabels[3], masterPanSlider,    60);
    positionMaster(masterLabels[4], masterPortaSlider,  60);
    positionMaster(masterLabels[5], masterAgeSlider,    60);
    positionMaster(masterLabels[6], masterFeedSlider,   60);
    positionMaster(masterLabels[7], masterPBSlider,     60);
    positionMaster(masterLabels[8], masterMWSlider,     60);
    masterLabelRow.removeFromLeft(8);
    masterKnobRow .removeFromLeft(8);
    positionMaster(masterLabels[9],  masterA, 60);
    positionMaster(masterLabels[10], masterD, 60);
    positionMaster(masterLabels[11], masterS, 60);
    positionMaster(masterLabels[12], masterR, 60);

    tabs.setBounds(area);

    // ── Oscillators tab ───────────────────────────────────────────────────────
    layoutOscillatorsTab();

    // ── FM ENGINE tab ─────────────────────────────────────────────────────────
    if (engineTab)
    {
        auto tabArea = engineTab->getLocalBounds().reduced(8);
        const int opW = juce::jmax(54, tabArea.getWidth() / 10);

        {
            auto titleRow = tabArea.removeFromTop(20);
            fmEnableButton.setBounds(titleRow.removeFromRight(48));
            fmEnableLabel .setBounds(titleRow.removeFromLeft(160));
        }
        tabArea.removeFromTop(4);

        auto topLabelRow = tabArea.removeFromTop(13);
        fmAlgoLabel.setBounds(topLabelRow.removeFromLeft(180));
        fmLvlLabel .setBounds(topLabelRow.removeFromLeft(80));
        fmOctLabel .setBounds(topLabelRow.removeFromLeft(80));
        topLabelRow.removeFromLeft(8);
        for (auto& label : fmAdsrLabels)
            label.setBounds(topLabelRow.removeFromLeft(48));

        auto topRow = tabArea.removeFromTop(84);
        fmAlgorithmBox.setBounds(topRow.removeFromLeft(180).withHeight(24));
        fmLevelSlider .setBounds(topRow.removeFromLeft(80));
        fmOctaveSlider.setBounds(topRow.removeFromLeft(80));
        topRow.removeFromLeft(8);
        fmA.setBounds(topRow.removeFromLeft(48).reduced(2, 0));
        fmD.setBounds(topRow.removeFromLeft(48).reduced(2, 0));
        fmS.setBounds(topRow.removeFromLeft(48).reduced(2, 0));
        fmR.setBounds(topRow.removeFromLeft(48).reduced(2, 0));

        tabArea.removeFromTop(6);

        const int labelH = 10;
        const int gap    = 2;
        const int waveH  = 22;
        const int sliderRows = 9;
        const int sliderH = juce::jlimit(18, 54,
            (tabArea.getHeight() - 13 - labelH - waveH - sliderRows * labelH - (sliderRows + 1) * gap)
                / sliderRows);

        auto opIndexRow = tabArea.removeFromTop(13);
        for (int i = 0; i < 10; ++i)
            fmOpLabels[(size_t)i].setBounds(opIndexRow.removeFromLeft(opW));

        auto layoutSliderRow = [&](juce::Label& header, auto& sliders)
        {
            header.setBounds(tabArea.removeFromTop(labelH));
            auto row = tabArea.removeFromTop(sliderH);
            for (int i = 0; i < 10; ++i)
                sliders[(size_t)i].setBounds(row.removeFromLeft(opW).reduced(2));
            tabArea.removeFromTop(gap);
        };

        layoutSliderRow(fmCoarseHdrLabel,  fmOpCoarseSliders);
        layoutSliderRow(fmFineHdrLabel,    fmOpFineSliders);
        layoutSliderRow(fmDetuneHdrLabel,  fmOpDetuneSliders);
        layoutSliderRow(fmLevelHdrLabel,   fmOpLevelSliders);
        layoutSliderRow(fmFeedbackHdrLabel,fmOpFeedbackSliders);

        fmWaveHdrLabel.setBounds(tabArea.removeFromTop(labelH));
        auto waveRow = tabArea.removeFromTop(waveH);
        for (int i = 0; i < 10; ++i)
            fmOpWaveBoxes[(size_t)i].setBounds(waveRow.removeFromLeft(opW).reduced(2, 0));
        tabArea.removeFromTop(gap);

        layoutSliderRow(fmSkirtHdrLabel, fmOpSkirtSliders);
        layoutSliderRow(fmResHdrLabel,   fmOpResSliders);
        layoutSliderRow(fmVelHdrLabel,   fmOpVelSliders);
        layoutSliderRow(fmKeyHdrLabel,   fmOpKeySliders);
    }

    // ── Filter / Mod / Arp tab ────────────────────────────────────────────────
    if (effectsTab && filterPanel && modPanel && arpPanel && lfoPanel)
    {
        auto tabArea = effectsTab->getLocalBounds().reduced(6);
        filterPanel->setBounds(tabArea.removeFromTop(240).reduced(3));
        tabArea.removeFromTop(4);
        int arpW = 240, lfoW = 280;
        arpPanel->setBounds(tabArea.removeFromRight(arpW).reduced(3));
        lfoPanel->setBounds(tabArea.removeFromRight(lfoW).reduced(3));
        modPanel->setBounds(tabArea.reduced(3));
    }
}
