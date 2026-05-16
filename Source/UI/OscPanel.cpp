#include "OscPanel.h"
#include <cmath>

// ── VAWaveDisplay ─────────────────────────────────────────────────────────────
// Shape indices match VA combo order: Sine=0 Saw=1 RevSaw=2 Square=3 Triangle=4 Pulse=5
void VAWaveDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced(2.f);
    g.setColour(juce::Colour(0xff0a0c0e));
    g.fillRoundedRectangle(b, 3.f);
    g.setColour(accent.withAlpha(0.35f));
    g.drawRoundedRectangle(b, 3.f, 1.f);

    const int kPts = 128;
    juce::Path wave;
    float cx = b.getX(), cy = b.getCentreY(), w = b.getWidth(), h = b.getHeight() * 0.42f;

    for (int i = 0; i <= kPts; ++i)
    {
        float t  = (float)i / kPts;
        float ph = std::fmod(t * 2.f, 1.f);   // two cycles for visual interest
        float v  = 0.f;
        switch (shape)
        {
            case 0: v = std::sin(ph * juce::MathConstants<float>::twoPi); break;
            case 1: v = 1.f - 2.f * ph;                                    break;
            case 2: v = 2.f * ph - 1.f;                                    break;
            case 3: v = ph < 0.5f ? 1.f : -1.f;                            break;
            case 4: v = (ph < 0.5f) ? (4.f * ph - 1.f) : (3.f - 4.f * ph); break;
            case 5: v = ph < 0.25f ? 1.f : -1.f;                           break;
            default: break;
        }
        float x = cx + t * w;
        float y = cy - v * h;
        if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
    }

    g.setColour(accent);
    g.strokePath(wave, juce::PathStrokeType(1.5f));
}

// ─────────────────────────────────────────────────────────────────────────────
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
        modeBoxA.addItemList({"SinSum","SinProd","Ripple","Torus",
                              "Vortex","Turbulence","Lorenz","Dipole",
                              "Folded","Chebyshev",
                              "Harmonic","Chord","Shell","Gravity",
                              "Formant","Metallic","Tension","Body"}, 1);
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
    else if (oscId == "ks")
    {
        setupExtraSlider(extra1, "osc_ks_decay",      "Decay (0=short, 1=long)",         extraAtt1);
        setupExtraSlider(extra2, "osc_ks_brightness", "Brightness (loop LPF blend)",      extraAtt2);
        setupExtraSlider(extra3, "osc_ks_stiffness",  "Stiffness (all-pass dispersion)",  extraAtt3);
        setupExtraSlider(extra4, "osc_ks_morph",      "Excitation morph (noise→glitch)",  extraAtt4);
        setupExtraSlider(extra5, "osc_ks_position",   "Pluck position",                   extraAtt5);
        numExtraSliders = 5;
    }
    else if (oscId == "grain")
    {
        modeBoxA.addItemList({"Free Cloud","Burst","Audio Rate"}, 1);
        modeBoxB.addItemList({"Hann","Triangle","Gaussian","Tukey","Rect","Exp Attack","Exp Decay"}, 1);
        modeBoxC.addItemList({"Off","Glass","Metal","Wood","Plate","String","Drum","Pipe","Alien"}, 1);
        modeBoxA.setTooltip("Grain Trigger Mode");
        modeBoxB.setTooltip("Grain Window");
        modeBoxC.setTooltip("Physical Body Type");
        IndustrialTheme::styleCombo(modeBoxA, accent);
        IndustrialTheme::styleCombo(modeBoxB, accent);
        IndustrialTheme::styleCombo(modeBoxC, accent);
        addAndMakeVisible(modeBoxA);
        addAndMakeVisible(modeBoxB);
        addAndMakeVisible(modeBoxC);
        modeAttA = std::make_unique<CAtt>(apvts, "osc_grain_trigger_mode", modeBoxA);
        modeAttB = std::make_unique<CAtt>(apvts, "osc_grain_window", modeBoxB);
        modeAttC = std::make_unique<CAtt>(apvts, "osc_grain_body_type", modeBoxC);
        numModeBoxes = 3;

        setupExtraSlider(extra1,  "osc_grain_position", "Grain Position",          extraAtt1);
        setupExtraSlider(extra2,  "osc_grain_size",     "Grain Size",              extraAtt2);
        setupExtraSlider(extra3,  "osc_grain_density",  "Grain Density",           extraAtt3);
        setupExtraSlider(extra4,  "osc_grain_pitch",    "Grain Pitch",             extraAtt4);
        setupExtraSlider(extra5,  "osc_grain_spray",    "Position Spray",          extraAtt5);
        setupExtraSlider(extra6,  "osc_grain_jitter",   "Timing / Pitch Jitter",   extraAtt6);
        setupExtraSlider(extra7,  "osc_grain_pan",      "Pan Spread",              extraAtt7);
        setupExtraSlider(extra8,  "osc_grain_reverse",  "Reverse Probability",     extraAtt8);
        setupExtraSlider(extra9,  "osc_grain_scan",     "Position Scan Speed",     extraAtt9);
        setupExtraSlider(extra10, "osc_grain_freeze",   "Position Freeze Amount",  extraAtt10);
        setupExtraSlider(extra11, "osc_grain_body_root",   "Body Root Pitch",       extraAtt11);
        setupExtraSlider(extra12, "osc_grain_body_size",   "Body Size",             extraAtt12);
        setupExtraSlider(extra13, "osc_grain_body_decay",  "Body Decay",            extraAtt13);
        setupExtraSlider(extra14, "osc_grain_body_damp",   "Body Damping",          extraAtt14);
        setupExtraSlider(extra15, "osc_grain_body_bright", "Body Brightness",       extraAtt15);
        setupExtraSlider(extra16, "osc_grain_body_inharm", "Body Inharmonicity",    extraAtt16);
        setupExtraSlider(extra17, "osc_grain_body_excite", "Body Excitation",       extraAtt17);
        setupExtraSlider(extra18, "osc_grain_body_mix",    "Body Mix",              extraAtt18);
        setupExtraSlider(extra19, "osc_grain_body_spread", "Body Stereo Spread",    extraAtt19);
        setupExtraSlider(extra20, "osc_grain_body_modes",  "Body Mode Count",       extraAtt20);
        numExtraSliders = 20;
    }
    else if (oscId.startsWith("va"))
    {
        setupExtraSlider(extra1, pfx + "shape",  "Wave Shape Morph (0=Sine 1=Saw 2=RevSaw 3=Sqr 4=Tri 5=Pulse)", extraAtt1);
        setupExtraSlider(extra2, pfx + "pw",     "Pulse Width",      extraAtt2);
        setupExtraSlider(extra3, pfx + "detune", "Unison Detune",    extraAtt3);
        setupExtraSlider(extra4, pfx + "unison", "Unison Voices",    extraAtt4);
        setupExtraSlider(extra5, pfx + "spread", "Stereo Spread",    extraAtt5);
        setupExtraSlider(extra6, pfx + "ring",   "Ring Modulation",  extraAtt6);
        setupExtraSlider(extra7, pfx + "fold",   "Wave Folding",     extraAtt7);
        numExtraSliders = 7;

        waveDisplay = std::make_unique<VAWaveDisplay>(accent);
        addAndMakeVisible(*waveDisplay);
        waveDisplay->setShape(juce::roundToInt(extra1.getValue()));
        extra1.onValueChange = [this] {
            waveDisplay->setShape(juce::roundToInt(
                juce::jlimit(extra1.getMinimum(), extra1.getMaximum(), extra1.getValue())));
        };
    }
    else if (oscId == "wt")
    {
        modeBoxA.addItemList({"Sine","Triangle","Saw","Square","Pulse",
                              "Half Sine","Abs Sine","Sine Cube",
                              "Hollow Odd","Even Glass","Vocal A","Metallic",
                              "Sync Saw","Reso Fold","Noisy Digital"}, 1);
        modeBoxB.addItemList({"Off","Bend+","Bend-","Bend +/-","Mirror",
                              "Sync","Window Sync","Quantize","Fold","Formant",
                              "PD Saw","PD Square","PD Pulse","PD Resonant","PD Formant"}, 1);
        modeBoxA.setTooltip("Wavetable Shape");
        modeBoxB.setTooltip("Wavetable Warp Mode");
        IndustrialTheme::styleCombo(modeBoxA, accent);
        IndustrialTheme::styleCombo(modeBoxB, accent);
        addAndMakeVisible(modeBoxA);
        addAndMakeVisible(modeBoxB);
        modeAttA = std::make_unique<CAtt>(apvts, "osc_wt_table", modeBoxA);
        modeAttB = std::make_unique<CAtt>(apvts, "osc_wt_warp_mode", modeBoxB);
        numModeBoxes = 2;

        setupExtraSlider(extra1, "osc_wt_pos",        "Wavetable Position",        extraAtt1);
        setupExtraSlider(extra2, "osc_wt_warp",       "Warp Amount",               extraAtt2);
        setupExtraSlider(extra3, "osc_wt_pd_break",   "Phase Distortion Breakpoint", extraAtt3);
        setupExtraSlider(extra4, "osc_wt_unison",     "Unison Voices",             extraAtt4);
        setupExtraSlider(extra5, "osc_wt_detune",     "Unison Detune",             extraAtt5);
        setupExtraSlider(extra6, "osc_wt_spread",     "Stereo Spread",             extraAtt6);
        setupExtraSlider(extra7, "osc_wt_pos_spread", "Wavetable Position Spread", extraAtt7);
        setupExtraSlider(extra8, "osc_wt_phase",      "Phase Offset",              extraAtt8);
        setupExtraSlider(extra9, "osc_wt_random",     "Random Phase Amount",       extraAtt9);
        numExtraSliders = 9;

        retrigButton.setButtonText("RT");
        retrigButton.setTooltip("Retrigger WT phase on note-on");
        retrigButton.setColour(juce::ToggleButton::textColourId, accent);
        retrigButton.setColour(juce::ToggleButton::tickColourId, accent);
        retrigButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff555555));
        addAndMakeVisible(retrigButton);
        retrigAtt = std::make_unique<BAtt>(apvts, "osc_wt_retrig", retrigButton);
        hasRetrigButton = true;
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
    else if (oscId == "ks")
    {
        makeLabel(extraLabels[0], "DECAY");
        makeLabel(extraLabels[1], "BRIGHT");
        makeLabel(extraLabels[2], "STIFF");
        makeLabel(extraLabels[3], "MORPH");
        makeLabel(extraLabels[4], "POS");
    }
    else if (oscId == "grain")
    {
        makeLabel(modeALabel, "TRIGGER");
        makeLabel(modeBLabel, "WINDOW");
        makeLabel(modeCLabel, "BODY");
        makeLabel(extraLabels[0], "POS");
        makeLabel(extraLabels[1], "SIZE");
        makeLabel(extraLabels[2], "DENS");
        makeLabel(extraLabels[3], "PITCH");
        makeLabel(extraLabels[4], "SPRAY");
        makeLabel(extraLabels[5], "JITTR");
        makeLabel(extraLabels[6], "PAN");
        makeLabel(extraLabels[7], "REV");
        makeLabel(extraLabels[8], "SCAN");
        makeLabel(extraLabels[9], "FREEZE");
        makeLabel(extraLabels[10], "ROOT");
        makeLabel(extraLabels[11], "BODY-SZ");
        makeLabel(extraLabels[12], "DECAY");
        makeLabel(extraLabels[13], "DAMP");
        makeLabel(extraLabels[14], "BRIGHT");
        makeLabel(extraLabels[15], "INHARM");
        makeLabel(extraLabels[16], "EXCITE");
        makeLabel(extraLabels[17], "B-MIX");
        makeLabel(extraLabels[18], "B-SPR");
        makeLabel(extraLabels[19], "MODES");
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
        makeLabel(modeBLabel, "WARP");
        makeLabel(extraLabels[0], "POS");
        makeLabel(extraLabels[1], "WARP");
        makeLabel(extraLabels[2], "PD-BRK");
        makeLabel(extraLabels[3], "UNI");
        makeLabel(extraLabels[4], "DETUNE");
        makeLabel(extraLabels[5], "SPREAD");
        makeLabel(extraLabels[6], "P-SPR");
        makeLabel(extraLabels[7], "PHASE");
        makeLabel(extraLabels[8], "RAND");
        makeLabel(retrigLabel, "RETRIG");
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

    // ── VA Waveform display ───────────────────────────────────────────────────
    if (waveDisplay != nullptr)
    {
        waveDisplay->setBounds(area.removeFromTop(52));
        area.removeFromTop(2);
    }

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
        if (n > 2)
        {
            modeCLabel.setBounds(portion(lr, 2, n));
            modeBoxC  .setBounds(portion(mr, 2, n).reduced(1, 0));
        }
        area.removeFromTop(4);
    }

    if (hasRetrigButton)
    {
        auto rr = area.removeFromTop(22);
        retrigLabel.setBounds(rr.removeFromLeft(58));
        retrigButton.setBounds(rr.removeFromLeft(50));
        area.removeFromTop(4);
    }

    // ── Extra Sliders ─────────────────────────────────────────────────────────
    if (numExtraSliders > 0)
    {
        std::array<juce::Slider*, 20> sliders {
            &extra1, &extra2, &extra3, &extra4, &extra5,
            &extra6, &extra7, &extra8, &extra9, &extra10,
            &extra11, &extra12, &extra13, &extra14, &extra15,
            &extra16, &extra17, &extra18, &extra19, &extra20
        };

        const int cols = numExtraSliders > 12 ? 4 : 3;
        const int rows = (numExtraSliders + cols - 1) / cols;
        const int reservedForAdsr = 58;
        const int available = juce::jmax(0, area.getHeight() - reservedForAdsr);
        const int rowSliderH = juce::jlimit(30, 70, (available - rows * 15) / juce::jmax(1, rows));

        for (int row = 0; row < rows; ++row)
        {
            if (row > 0) area.removeFromTop(2);
            const int start = row * cols;
            const int n = juce::jmin(cols, numExtraSliders - start);
            auto lr = area.removeFromTop(13);
            auto sr = area.removeFromTop(rowSliderH);
            for (int i = 0; i < n; ++i)
            {
                const int index = start + i;
                extraLabels[(size_t)index].setBounds(portion(lr, i, n));
                sliders[(size_t)index]->setBounds(portion(sr, i, n));
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
