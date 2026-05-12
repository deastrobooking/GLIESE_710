#include "ArpPanel.h"

ArpPanel::ArpPanel(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    titleLabel.setText("ARPEGGIATOR", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::lightcyan);
    titleLabel.setFont(juce::Font(13.f, juce::Font::bold));
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
    auto area = getLocalBounds().reduced(6);
    titleLabel.setBounds(area.removeFromTop(20));

    auto row1 = area.removeFromTop(24);
    onToggle.setBounds(row1.removeFromLeft(40));
    row1.removeFromLeft(4);
    syncToggle.setBounds(row1.removeFromLeft(50));
    row1.removeFromLeft(4);
    latchToggle.setBounds(row1.removeFromLeft(50));

    area.removeFromTop(4);
    auto row2 = area.removeFromTop(24);
    patternBox.setBounds(row2.removeFromLeft(row2.getWidth() / 2 - 2));
    row2.removeFromLeft(4);
    rateBox.setBounds(row2);

    area.removeFromTop(6);
    int sw = area.getWidth() / 3;
    octavesSlider.setBounds(area.removeFromLeft(sw));
    gateSlider   .setBounds(area.removeFromLeft(sw));
    swingSlider  .setBounds(area);
}
