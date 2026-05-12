#include "OscPanel.h"

OscPanel::OscPanel(juce::AudioProcessorValueTreeState& a, const juce::String& id,
                   const juce::String& lbl)
    : apvts(a), oscId(id), label(lbl)
{
    titleLabel.setText(lbl, juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::Font(juce::FontOptions { 14.f, juce::Font::bold }));
    addAndMakeVisible(titleLabel);

    juce::String pfx = "osc_" + oscId + "_";

    setupSlider(levelSlider,  "Oscillator Level");
    levelAtt = std::make_unique<Att>(apvts, pfx + "level", levelSlider);

    setupSlider(octaveSlider, "Octave Transpose");
    octaveAtt = std::make_unique<Att>(apvts, pfx + "octave", octaveSlider);

    setupSlider(adsrA, "Attack");  attA = std::make_unique<Att>(apvts, pfx + "adsr_a", adsrA);
    setupSlider(adsrD, "Decay");   attD = std::make_unique<Att>(apvts, pfx + "adsr_d", adsrD);
    setupSlider(adsrS, "Sustain"); attS = std::make_unique<Att>(apvts, pfx + "adsr_s", adsrS);
    setupSlider(adsrR, "Release"); attR = std::make_unique<Att>(apvts, pfx + "adsr_r", adsrR);

    if (oscId == "3d")
    {
        modeBoxA.addItemList({"SinSum","SinProduct","Ripple","Torus","Vortex",
                              "Noise","Lorenz","Dipole","Folded","Chebyshev"}, 1);
        modeBoxB.addItemList({"Circle","Spiral","Lissajous","Random","Orbit"}, 1);
        modeBoxA.setTooltip("3D Field");
        modeBoxB.setTooltip("3D Path");
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
    else if (oscId == "va")
    {
        modeBoxA.addItemList({"Sine","Saw","RevSaw","Square","Triangle","Pulse"}, 1);
        modeBoxA.setTooltip("VA Shape");
        addAndMakeVisible(modeBoxA);
        modeAttA = std::make_unique<CAtt>(apvts, "osc_va_shape", modeBoxA);
        numModeBoxes = 1;

        setupExtraSlider(extra1, "osc_va_pw",     "Pulse Width",    extraAtt1);
        setupExtraSlider(extra2, "osc_va_detune", "Unison Detune",  extraAtt2);
        setupExtraSlider(extra3, "osc_va_unison", "Unison Voices",  extraAtt3);
        setupExtraSlider(extra4, "osc_va_spread", "Stereo Spread",  extraAtt4);
        numExtraSliders = 4;
    }
    else if (oscId == "wt")
    {
        modeBoxA.addItemList({"Sine","Triangle","Saw","Square","Pulse",
                              "Half Sine","Abs Sine","Sine Cube"}, 1);
        modeBoxA.setTooltip("Wavetable Shape");
        addAndMakeVisible(modeBoxA);
        modeAttA = std::make_unique<CAtt>(apvts, "osc_wt_table", modeBoxA);
        numModeBoxes = 1;

        setupExtraSlider(extra1, "osc_wt_pos", "Wavetable Position", extraAtt1);
        numExtraSliders = 1;
    }

    // ── Labels ────────────────────────────────────────────────────────────────
    auto makeLabel = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colour(0xffcc2222));
        l.setFont(juce::Font(juce::FontOptions { 10.f }));
        l.setJustificationType(juce::Justification::centred);
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
    else if (oscId == "va")
    {
        makeLabel(modeALabel, "SHAPE");
        makeLabel(extraLabels[0], "PW");
        makeLabel(extraLabels[1], "DETUNE");
        makeLabel(extraLabels[2], "UNISON");
        makeLabel(extraLabels[3], "SPREAD");
    }
    else if (oscId == "wt")
    {
        makeLabel(modeALabel, "SHAPE");
        makeLabel(extraLabels[0], "POS");
    }

    adsrLabel.setText("A  D  S  R", juce::dontSendNotification);
    adsrLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcc2222));
    adsrLabel.setFont(juce::Font(juce::FontOptions { 10.f }));
    adsrLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(adsrLabel);
}

void OscPanel::setupSlider(juce::Slider& s, const juce::String& tooltip)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    s.setTooltip(tooltip);
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
    g.setColour(juce::Colour(0xff141414));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.f);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.f, 1.f);
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
    titleLabel.setBounds(area.removeFromTop(20));
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

    // ── ADSR ──────────────────────────────────────────────────────────────────
    adsrLabel.setBounds(area.removeFromTop(14));
    auto adsrRow = area.removeFromTop(80);
    juce::Slider* adsrSliders[4] = { &adsrA, &adsrD, &adsrS, &adsrR };
    for (int i = 0; i < 4; ++i)
        adsrSliders[i]->setBounds(portion(adsrRow, i, 4));
}
