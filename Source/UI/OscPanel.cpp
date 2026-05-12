#include "OscPanel.h"

OscPanel::OscPanel(juce::AudioProcessorValueTreeState& a, const juce::String& id,
                   const juce::String& lbl)
    : apvts(a), oscId(id), label(lbl)
{
    titleLabel.setText(lbl, juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::lightcyan);
    titleLabel.setFont(juce::Font(juce::FontOptions { 14.f, juce::Font::bold }));
    addAndMakeVisible(titleLabel);

    juce::String pfx = "osc_" + oscId + "_";

    setupSlider(levelSlider, "Oscillator Level");
    levelAtt = std::make_unique<Att>(apvts, pfx + "level", levelSlider);

    setupSlider(octaveSlider, "Octave Transpose");
    octaveAtt = std::make_unique<Att>(apvts, pfx + "octave", octaveSlider);

    setupSlider(adsrA, "Attack");
    attA = std::make_unique<Att>(apvts, pfx + "adsr_a", adsrA);
    setupSlider(adsrD, "Decay");
    attD = std::make_unique<Att>(apvts, pfx + "adsr_d", adsrD);
    setupSlider(adsrS, "Sustain");
    attS = std::make_unique<Att>(apvts, pfx + "adsr_s", adsrS);
    setupSlider(adsrR, "Release");
    attR = std::make_unique<Att>(apvts, pfx + "adsr_r", adsrR);

    if (oscId == "3d")
    {
        modeBoxA.addItemList({"SinSum", "SinProduct", "Ripple", "Torus", "Vortex", "Noise",
                              "Lorenz", "Dipole", "Folded", "Chebyshev"}, 1);
        modeBoxB.addItemList({"Circle", "Spiral", "Lissajous", "Random", "Orbit"}, 1);
        modeBoxA.setTooltip("3D Field");
        modeBoxB.setTooltip("3D Path");
        addAndMakeVisible(modeBoxA);
        addAndMakeVisible(modeBoxB);
        modeAttA = std::make_unique<CAtt>(apvts, "osc_3d_field", modeBoxA);
        modeAttB = std::make_unique<CAtt>(apvts, "osc_3d_path", modeBoxB);
        numModeBoxes = 2;

        setupExtraSlider(extra1, "osc_3d_radius", "3D Radius", extraAtt1);
        setupExtraSlider(extra2, "osc_3d_depth", "3D Depth", extraAtt2);
        setupExtraSlider(extra3, "osc_3d_warp", "3D Warp", extraAtt3);
        setupExtraSlider(extra4, "osc_3d_rotation", "3D Rotation", extraAtt4);
        setupExtraSlider(extra5, "osc_3d_chaos", "3D Chaos", extraAtt5);
        setupExtraSlider(extra6, "osc_3d_morph", "3D Morph", extraAtt6);
        setupExtraSlider(extra7, "osc_3d_field_freq", "Field Frequency", extraAtt7);
        setupExtraSlider(extra8, "osc_3d_z_ratio", "Z Ratio", extraAtt8);
        setupExtraSlider(extra9, "osc_3d_fold", "Coordinate Fold", extraAtt9);
        setupExtraSlider(extra10, "osc_3d_drive", "Field Drive", extraAtt10);
        numExtraSliders = 10;
    }
    else if (oscId == "va")
    {
        modeBoxA.addItemList({"Sine", "Saw", "RevSaw", "Square", "Triangle", "Pulse"}, 1);
        modeBoxA.setTooltip("VA Shape");
        addAndMakeVisible(modeBoxA);
        modeAttA = std::make_unique<CAtt>(apvts, "osc_va_shape", modeBoxA);
        numModeBoxes = 1;

        setupExtraSlider(extra1, "osc_va_pw", "Pulse Width", extraAtt1);
        setupExtraSlider(extra2, "osc_va_detune", "Unison Detune", extraAtt2);
        setupExtraSlider(extra3, "osc_va_unison", "Unison Voices", extraAtt3);
        setupExtraSlider(extra4, "osc_va_spread", "Stereo Spread", extraAtt4);
        numExtraSliders = 4;
    }
    else if (oscId == "wt")
    {
        setupExtraSlider(extra1, "osc_wt_table", "Wavetable Index", extraAtt1);
        setupExtraSlider(extra2, "osc_wt_pos", "Wavetable Position", extraAtt2);
        numExtraSliders = 2;
    }

    adsrLabel.setText("A  D  S  R", juce::dontSendNotification);
    adsrLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    adsrLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(adsrLabel);
    addAndMakeVisible(levelLabel);
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
    g.setColour(juce::Colour(0xff1a1a28));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.f);
    g.setColour(juce::Colour(0xff3a3a5a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.f, 1.f);
}

void OscPanel::resized()
{
    auto area = getLocalBounds().reduced(6);
    titleLabel.setBounds(area.removeFromTop(20));

    auto topRow = area.removeFromTop(70);
    int sliderW = topRow.getWidth() / 2;
    levelSlider .setBounds(topRow.removeFromLeft(sliderW));
    octaveSlider.setBounds(topRow);

    area.removeFromTop(4);

    if (numModeBoxes > 0)
    {
        auto modeRow = area.removeFromTop(24);
        if (numModeBoxes == 1)
            modeBoxA.setBounds(modeRow);
        else
        {
            int modeW = modeRow.getWidth() / 2;
            modeBoxA.setBounds(modeRow.removeFromLeft(modeW).reduced(1, 0));
            modeBoxB.setBounds(modeRow.reduced(1, 0));
        }
        area.removeFromTop(4);
    }

    if (numExtraSliders > 0)
    {
        auto firstRow = area.removeFromTop(70);
        int firstCount = juce::jmin(3, numExtraSliders);
        int firstW = firstRow.getWidth() / firstCount;
        if (numExtraSliders >= 1) extra1.setBounds(firstRow.removeFromLeft(firstW));
        if (numExtraSliders >= 2) extra2.setBounds(firstRow.removeFromLeft(firstW));
        if (numExtraSliders >= 3) extra3.setBounds(firstRow);

        if (numExtraSliders > 3)
        {
            area.removeFromTop(2);
            auto secondRow = area.removeFromTop(70);
            int secondCount = juce::jmin(3, numExtraSliders - 3);
            int secondW = secondRow.getWidth() / secondCount;
            if (numExtraSliders >= 4) extra4.setBounds(secondRow.removeFromLeft(secondW));
            if (numExtraSliders >= 5) extra5.setBounds(secondRow.removeFromLeft(secondW));
            if (numExtraSliders >= 6) extra6.setBounds(secondRow);
        }

        if (numExtraSliders > 6)
        {
            area.removeFromTop(2);
            auto thirdRow = area.removeFromTop(70);
            int thirdCount = numExtraSliders - 6;
            int thirdW = thirdRow.getWidth() / thirdCount;
            if (numExtraSliders >= 7) extra7.setBounds(thirdRow.removeFromLeft(thirdW));
            if (numExtraSliders >= 8) extra8.setBounds(thirdRow.removeFromLeft(thirdW));
            if (numExtraSliders >= 9) extra9.setBounds(thirdRow.removeFromLeft(thirdW));
            if (numExtraSliders >= 10) extra10.setBounds(thirdRow);
        }
        area.removeFromTop(4);
    }

    adsrLabel.setBounds(area.removeFromTop(16));

    auto adsrRow = area.removeFromTop(80);
    int w = adsrRow.getWidth() / 4;
    adsrA.setBounds(adsrRow.removeFromLeft(w));
    adsrD.setBounds(adsrRow.removeFromLeft(w));
    adsrS.setBounds(adsrRow.removeFromLeft(w));
    adsrR.setBounds(adsrRow);
}
