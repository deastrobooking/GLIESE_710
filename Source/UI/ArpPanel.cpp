#include "ArpPanel.h"

ArpPanel::ArpPanel(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    titleLabel.setText("ARPEGGIATOR", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::lightcyan);
    titleLabel.setFont(juce::Font(juce::FontOptions { 13.f, juce::Font::bold }));
    addAndMakeVisible(titleLabel);

    onToggle.setButtonText("ON");
    addAndMakeVisible(onToggle);
    onAtt = std::make_unique<BAtt>(apvts, "arp_on", onToggle);

    patternBox.addItemList({"Up","Down","Up-Down","Down-Up","As Played","Random","Random Latch"}, 1);
    addAndMakeVisible(patternBox);
    patternAtt = std::make_unique<CAtt>(apvts, "arp_pattern", patternBox);

    rateBox.addItemList({"1/32","1/16","1/8","3/16","1/4","3/8","1/2","3/4","1 Bar"}, 1);
    addAndMakeVisible(rateBox);
    rateAtt = std::make_unique<CAtt>(apvts, "arp_rate", rateBox);

    syncToggle.setButtonText("Sync");
    addAndMakeVisible(syncToggle);
    syncAtt = std::make_unique<BAtt>(apvts, "arp_sync", syncToggle);

    latchToggle.setButtonText("Latch");
    addAndMakeVisible(latchToggle);
    latchAtt = std::make_unique<BAtt>(apvts, "arp_latch", latchToggle);

    auto setupSlider = [&](juce::Slider& s, const juce::String& tip) {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
        s.setTooltip(tip);
        addAndMakeVisible(s);
    };

    setupSlider(octavesSlider, "Octave Range");
    octAtt = std::make_unique<Att>(apvts, "arp_octaves", octavesSlider);

    setupSlider(gateSlider, "Gate Length");
    gateAtt = std::make_unique<Att>(apvts, "arp_gate", gateSlider);

    setupSlider(swingSlider, "Swing");
    swingAtt = std::make_unique<Att>(apvts, "arp_swing", swingSlider);

    auto makeLabel = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colour(0xff9999bb));
        l.setFont(juce::Font(juce::FontOptions { 10.f }));
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };

    makeLabel(patternLabel, "PATTERN");
    makeLabel(rateLabel,    "RATE");
    makeLabel(octLabel,     "OCT");
    makeLabel(gateLabel,    "GATE");
    makeLabel(swingLabel,   "SWING");
}

void ArpPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff1a1a28));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.f);
    g.setColour(juce::Colour(0xff3a3a5a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.f, 1.f);
}

void ArpPanel::resized()
{
    auto portion = [](juce::Rectangle<int> r, int i, int n) {
        int x  = r.getX() + r.getWidth() * i / n;
        int x2 = r.getX() + r.getWidth() * (i + 1) / n;
        return r.withX(x).withWidth(x2 - x);
    };

    auto area = getLocalBounds().reduced(6);
    titleLabel.setBounds(area.removeFromTop(20));

    // ON / Sync / Latch toggles (buttons carry their own text)
    auto row1 = area.removeFromTop(24);
    onToggle.setBounds(row1.removeFromLeft(40));
    row1.removeFromLeft(4);
    syncToggle.setBounds(row1.removeFromLeft(50));
    row1.removeFromLeft(4);
    latchToggle.setBounds(row1.removeFromLeft(50));

    area.removeFromTop(4);

    // Pattern / Rate labels + combos
    auto comboLabelRow = area.removeFromTop(13);
    patternLabel.setBounds(portion(comboLabelRow, 0, 2));
    rateLabel   .setBounds(portion(comboLabelRow, 1, 2));

    auto row2 = area.removeFromTop(24);
    patternBox.setBounds(portion(row2, 0, 2).reduced(2, 0));
    rateBox   .setBounds(portion(row2, 1, 2).reduced(2, 0));

    area.removeFromTop(4);

    // Oct / Gate / Swing labels + sliders
    auto sliderLabelRow = area.removeFromTop(13);
    octLabel  .setBounds(portion(sliderLabelRow, 0, 3));
    gateLabel .setBounds(portion(sliderLabelRow, 1, 3));
    swingLabel.setBounds(portion(sliderLabelRow, 2, 3));

    octavesSlider.setBounds(portion(area, 0, 3));
    gateSlider   .setBounds(portion(area, 1, 3));
    swingSlider  .setBounds(portion(area, 2, 3));
}
