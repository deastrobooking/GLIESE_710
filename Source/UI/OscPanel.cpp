#include "OscPanel.h"

OscPanel::OscPanel(juce::AudioProcessorValueTreeState& a, const juce::String& id,
                   const juce::String& lbl)
    : apvts(a), oscId(id), label(lbl)
{
    titleLabel.setText(lbl, juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::lightcyan);
    titleLabel.setFont(juce::Font(14.f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    auto setupSlider = [&](juce::Slider& s, const juce::String& tooltip) {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        s.setTooltip(tooltip);
        addAndMakeVisible(s);
    };

    juce::String pfx = "osc_" + oscId + "_";

    setupSlider(levelSlider, "Oscillator Level");
    levelAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, pfx + "level", levelSlider);

    setupSlider(octaveSlider, "Octave Transpose");
    octaveAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, pfx + "octave", octaveSlider);

    setupSlider(adsrA, "Attack");
    attA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, pfx + "adsr_a", adsrA);
    setupSlider(adsrD, "Decay");
    attD = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, pfx + "adsr_d", adsrD);
    setupSlider(adsrS, "Sustain");
    attS = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, pfx + "adsr_s", adsrS);
    setupSlider(adsrR, "Release");
    attR = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, pfx + "adsr_r", adsrR);

    adsrLabel.setText("A  D  S  R", juce::dontSendNotification);
    adsrLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    adsrLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(adsrLabel);
    addAndMakeVisible(levelLabel);
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
    adsrLabel.setBounds(area.removeFromTop(16));

    auto adsrRow = area.removeFromTop(80);
    int w = adsrRow.getWidth() / 4;
    adsrA.setBounds(adsrRow.removeFromLeft(w));
    adsrD.setBounds(adsrRow.removeFromLeft(w));
    adsrS.setBounds(adsrRow.removeFromLeft(w));
    adsrR.setBounds(adsrRow);
}
