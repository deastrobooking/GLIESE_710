#include "OscPanel.h"

OscPanel::OscPanel(juce::AudioProcessorValueTreeState& a, const juce::String& id,
                   const juce::String& lbl)
    : apvts(a), oscId(id), label(lbl), accent(IndustrialTheme::accentForOsc(id))
{
    titleLabel.setText(lbl, juce::dontSendNotification);
    IndustrialTheme::styleTitle(titleLabel, accent, 14.f);
    addAndMakeVisible(titleLabel);

    juce::String pfx = "osc_" + oscId + "_";

    // Enable / hard-shutoff toggle
    enableButton.setButtonText("ON");
    enableButton.setTooltip("Enable / disable this oscillator engine");
    enableButton.setColour(juce::ToggleButton::textColourId,    accent);
    enableButton.setColour(juce::ToggleButton::tickColourId,    accent);
    enableButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff555555));
    addAndMakeVisible(enableButton);
    enableAtt = std::make_unique<BAtt>(apvts, pfx + "on", enableButton);

    setupSlider(levelSlider,  "Oscillator Level");
    levelAtt = std::make_unique<Att>(apvts, pfx + "level", levelSlider);

    setupSlider(octaveSlider, "Octave Transpose");
    octaveAtt = std::make_unique<Att>(apvts, pfx + "octave", octaveSlider);

    setupADSRSlider(adsrA, "Attack");  attA = std::make_unique<Att>(apvts, pfx + "adsr_a", adsrA);
    setupADSRSlider(adsrD, "Decay");   attD = std::make_unique<Att>(apvts, pfx + "adsr_d", adsrD);
    setupADSRSlider(adsrS, "Sustain"); attS = std::make_unique<Att>(apvts, pfx + "adsr_s", adsrS);
    setupADSRSlider(adsrR, "Release"); attR = std::make_unique<Att>(apvts, pfx + "adsr_r", adsrR);

    if (oscId == "3d")
    {
        modeBoxA.addItemList({"SinSum","SinProduct","Ripple","Torus","Vortex",
                              "Noise","Lorenz","Dipole","Folded","Chebyshev"}, 1);
        modeBoxB.addItemList({"Circle","Spiral","Lissajous","Random","Orbit"}, 1);
        modeBoxA.setTooltip("3D Field");
        modeBoxB.setTooltip("3D Path");
        IndustrialTheme::styleCombo(modeBoxA, accent);
        IndustrialTheme::styleCombo(modeBoxB, accent);
        addAndMakeVisible(modeBoxA);
        addAndMakeVisible(modeBoxB);
        modeAttA = std::make_unique<CAtt>(apvts, "osc_3d_field", modeBoxA);
        modeAttB = std::make_unique<CAtt>(apvts, "osc_3d_path",  modeBoxB);
        numModeBoxes = 2;

        setupExtraSlider(extra1,  "osc_3d_radius",    "3D Radius",       extraAtt1);
        setupExtraSlider(extra2,  "osc_3d_depth",     "3D Depth",        extraAtt2);
        setupExtraSlider(extra3,  "osc_3d_warp",      "3D Warp",         extraAtt3);
        setupExtraSlider(extra4,  "osc_3d_rotation",  "3D Rotation",     extraAtt4);
        setupExtraSlider(extra5,  "osc_3d_chaos",     "3D Chaos",        extraAtt5);
        setupExtraSlider(extra6,  "osc_3d_morph",     "3D Morph",        extraAtt6);
        setupExtraSlider(extra7,  "osc_3d_field_freq","Field Frequency",  extraAtt7);
        setupExtraSlider(extra8,  "osc_3d_z_ratio",   "Z Ratio",          extraAtt8);
        setupExtraSlider(extra9,  "osc_3d_fold",      "Coordinate Fold",  extraAtt9);
        setupExtraSlider(extra10, "osc_3d_drive",     "Field Drive",      extraAtt10);
        numExtraSliders = 10;
    }
    else if (oscId.startsWith("va"))
    {
        setupExtraSlider(extra1, pfx + "shape",  "Wave Shape Morph", extraAtt1);
        setupExtraSlider(extra2, pfx + "pw",     "Pulse Width",      extraAtt2);
        setupExtraSlider(extra3, pfx + "detune", "Unison Detune",    extraAtt3);
        setupExtraSlider(extra4, pfx + "unison", "Unison Voices",    extraAtt4);
        setupExtraSlider(extra5, pfx + "spread", "Stereo Spread",    extraAtt5);
        setupExtraSlider(extra6, pfx + "ring",   "Ring Modulation",  extraAtt6);
        setupExtraSlider(extra7, pfx + "fold",   "Wave Folding",     extraAtt7);
        numExtraSliders = 7;
    }
    else if (oscId == "wt")
    {
        modeBoxA.addItemList({"Sine","Triangle","Saw","Square","Pulse",
                              "Half Sine","Abs Sine","Sine Cube"}, 1);
        modeBoxA.setTooltip("Wavetable Shape");
        IndustrialTheme::styleCombo(modeBoxA, accent);
        addAndMakeVisible(modeBoxA);
        modeAttA = std::make_unique<CAtt>(apvts, "osc_wt_table", modeBoxA);
        numModeBoxes = 1;

        setupExtraSlider(extra1, "osc_wt_pos", "Wavetable Position", extraAtt1);
        numExtraSliders = 1;
    }

    // ── Labels ────────────────────────────────────────────────────────────────
    auto makeLabel = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, accent, 10.f);
        addAndMakeVisible(l);
    };

    makeLabel(levelLabel,  "LEVEL");
    makeLabel(octaveLabel, "OCTAVE");

    if (oscId == "3d")
    {
        makeLabel(modeALabel, "FIELD");
        makeLabel(modeBLabel, "PATH");
        makeLabel(extraLabels[0], "RADIUS");
        makeLabel(extraLabels[1], "DEPTH");
        makeLabel(extraLabels[2], "WARP");
        makeLabel(extraLabels[3], "ROTAT");
        makeLabel(extraLabels[4], "CHAOS");
        makeLabel(extraLabels[5], "MORPH");
        makeLabel(extraLabels[6], "FREQ");
        makeLabel(extraLabels[7], "Z-RAT");
        makeLabel(extraLabels[8], "FOLD");
        makeLabel(extraLabels[9], "DRIVE");
    }
    else if (oscId.startsWith("va"))
    {
        makeLabel(extraLabels[0], "SHAPE");
        makeLabel(extraLabels[1], "PW");
        makeLabel(extraLabels[2], "DETUNE");
        makeLabel(extraLabels[3], "UNISON");
        makeLabel(extraLabels[4], "SPREAD");
        makeLabel(extraLabels[5], "RING");
        makeLabel(extraLabels[6], "FOLD");
    }
    else if (oscId == "wt")
    {
        makeLabel(modeALabel, "SHAPE");
        makeLabel(extraLabels[0], "POS");
    }

    const char* adsrNames[4] = { "A", "D", "S", "R" };
    for (int i = 0; i < 4; ++i)
    {
        adsrLabels[(size_t)i].setText(adsrNames[i], juce::dontSendNotification);
        IndustrialTheme::styleLabel(adsrLabels[(size_t)i], accent, 10.f);
        addAndMakeVisible(adsrLabels[(size_t)i]);
    }
}

void OscPanel::setupSlider(juce::Slider& s, const juce::String& tooltip)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    s.setTooltip(tooltip);
    IndustrialTheme::styleSlider(s, accent);
    addAndMakeVisible(s);
}

void OscPanel::setupADSRSlider(juce::Slider& s, const juce::String& tooltip)
{
    s.setSliderStyle(juce::Slider::LinearVertical);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    s.setTooltip(tooltip);
    IndustrialTheme::styleSlider(s, accent);
    addAndMakeVisible(s);
}

void OscPanel::setupExtraSlider(juce::Slider& slider, const juce::String& paramId,
                                const juce::String& tooltip, std::unique_ptr<Att>& attachment)
{
    setupSlider(slider, tooltip);
    attachment = std::make_unique<Att>(apvts, paramId, slider);
}

void OscPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintPanel(g, getLocalBounds(), accent, oscId == "3d");
}

void OscPanel::resized()
{
    // Evenly distributes rect 'r' into n equal slices, returning the i-th
    auto portion = [](juce::Rectangle<int> r, int i, int n) {
        int x  = r.getX() + r.getWidth() * i / n;
        int x2 = r.getX() + r.getWidth() * (i + 1) / n;
        return r.withX(x).withWidth(x2 - x);
    };

    auto area = getLocalBounds().reduced(6);
    {
        auto titleRow = area.removeFromTop(20);
        enableButton.setBounds(titleRow.removeFromRight(44));
        titleLabel  .setBounds(titleRow);
    }
    area.removeFromTop(2);

    // ── Level + Octave ────────────────────────────────────────────────────────
    auto lr0 = area.removeFromTop(13);
    auto sr0 = area.removeFromTop(70);
    levelLabel  .setBounds(portion(lr0, 0, 2));
    octaveLabel .setBounds(portion(lr0, 1, 2));
    levelSlider .setBounds(portion(sr0, 0, 2));
    octaveSlider.setBounds(portion(sr0, 1, 2));
    area.removeFromTop(4);

    // ── Mode Combo Boxes ──────────────────────────────────────────────────────
    if (numModeBoxes > 0)
    {
        int n = numModeBoxes;
        auto lr = area.removeFromTop(13);
        auto mr = area.removeFromTop(24);
        modeALabel.setBounds(portion(lr, 0, n));
        modeBoxA  .setBounds(portion(mr, 0, n).reduced(1, 0));
        if (n > 1)
        {
            modeBLabel.setBounds(portion(lr, 1, n));
            modeBoxB  .setBounds(portion(mr, 1, n).reduced(1, 0));
        }
        area.removeFromTop(4);
    }

    // ── Extra Sliders ─────────────────────────────────────────────────────────
    if (numExtraSliders > 0)
    {
        // Row 1: extra1–extra3 (label indices 0–2)
        int n1 = juce::jmin(3, numExtraSliders);
        {
            auto lr = area.removeFromTop(13);
            auto sr = area.removeFromTop(70);
            juce::Slider* sl[3] = { &extra1, &extra2, &extra3 };
            for (int i = 0; i < n1; ++i)
            {
                extraLabels[(size_t)i].setBounds(portion(lr, i, n1));
                sl[i]->setBounds(portion(sr, i, n1));
            }
        }

        // Row 2: extra4–extra6 (label indices 3–5)
        if (numExtraSliders > 3)
        {
            area.removeFromTop(2);
            int n2 = juce::jmin(3, numExtraSliders - 3);
            auto lr = area.removeFromTop(13);
            auto sr = area.removeFromTop(70);
            juce::Slider* sl[3] = { &extra4, &extra5, &extra6 };
            for (int i = 0; i < n2; ++i)
            {
                extraLabels[(size_t)(3 + i)].setBounds(portion(lr, i, n2));
                sl[i]->setBounds(portion(sr, i, n2));
            }
        }

        // Row 3: extra7–extra10 (label indices 6–9)
        if (numExtraSliders > 6)
        {
            area.removeFromTop(2);
            int n3 = numExtraSliders - 6;
            auto lr = area.removeFromTop(13);
            auto sr = area.removeFromTop(70);
            juce::Slider* sl[4] = { &extra7, &extra8, &extra9, &extra10 };
            for (int i = 0; i < n3; ++i)
            {
                extraLabels[(size_t)(6 + i)].setBounds(portion(lr, i, n3));
                sl[i]->setBounds(portion(sr, i, n3));
            }
        }
        area.removeFromTop(4);
    }

    // ── ADSR — vertical sliders filling remaining space ───────────────────────
    juce::Slider* adsrSliders[4] = { &adsrA, &adsrD, &adsrS, &adsrR };
    for (int i = 0; i < 4; ++i)
    {
        auto col = portion(area, i, 4).reduced(2, 0);
        adsrLabels[(size_t)i].setBounds(col.removeFromTop(13));
        adsrSliders[i]->setBounds(col);
    }
}
