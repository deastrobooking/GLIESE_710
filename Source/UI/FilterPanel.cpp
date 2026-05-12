#include "FilterPanel.h"

FilterPanel::FilterPanel(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    titleLabel.setText("DUAL FILTER", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::lightcyan);
    titleLabel.setFont(juce::Font(13.f, juce::Font::bold));
    addAndMakeVisible(titleLabel);

    auto setupCombo = [&](juce::ComboBox& cb) {
        cb.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(cb);
    };

    setupCombo(filterAType);
    typeAAtt = std::make_unique<CAtt>(apvts, "filter_a_type", filterAType);

    setupCombo(filterBType);
    typeBAtt = std::make_unique<CAtt>(apvts, "filter_b_type", filterBType);

    setupCombo(filterRouting);
    routingAtt = std::make_unique<CAtt>(apvts, "filter_routing", filterRouting);

    addSlider(cutoffA,     "filter_a_cutoff",    "Filter A Cutoff",    cutAAtt);
    addSlider(resonanceA,  "filter_a_resonance",  "Filter A Resonance", resAAtt);
    addSlider(driveA,      "filter_a_drive",      "Filter A Drive",     drvAAtt);
    addSlider(envAmtA,     "filter_a_env_amt",    "Filter A Env Amt",   envAAtt);
    addSlider(lfoAmtA,     "filter_a_lfo_amt",    "Filter A LFO Amt",   lfoAAtt);

    addSlider(cutoffB,     "filter_b_cutoff",    "Filter B Cutoff",    cutBAtt);
    addSlider(resonanceB,  "filter_b_resonance",  "Filter B Resonance", resBAtt);
    addSlider(driveB,      "filter_b_drive",      "Filter B Drive",     drvBAtt);
    addSlider(envAmtB,     "filter_b_env_amt",    "Filter B Env Amt",   envBAtt);
    addSlider(lfoAmtB,     "filter_b_lfo_amt",    "Filter B LFO Amt",   lfoBAtt);
}

void FilterPanel::addSlider(juce::Slider& s, const juce::String& paramId,
                              const juce::String& tip, std::unique_ptr<Att>& att)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 16);
    s.setTooltip(tip);
    addAndMakeVisible(s);
    att = std::make_unique<Att>(apvts, paramId, s);
}

void FilterPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff1a1a28));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.f);
    g.setColour(juce::Colour(0xff3a3a5a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.f, 1.f);
}

void FilterPanel::resized()
{
    auto area = getLocalBounds().reduced(6);
    titleLabel.setBounds(area.removeFromTop(20));

    auto topRow = area.removeFromTop(24);
    int w3 = topRow.getWidth() / 3;
    filterAType.setBounds(topRow.removeFromLeft(w3));
    filterBType.setBounds(topRow.removeFromLeft(w3));
    filterRouting.setBounds(topRow);

    area.removeFromTop(4);

    auto rowA = area.removeFromTop(80);
    auto rowB = area;
    int sw = rowA.getWidth() / 5;

    cutoffA   .setBounds(rowA.removeFromLeft(sw));
    resonanceA.setBounds(rowA.removeFromLeft(sw));
    driveA    .setBounds(rowA.removeFromLeft(sw));
    envAmtA   .setBounds(rowA.removeFromLeft(sw));
    lfoAmtA   .setBounds(rowA);

    cutoffB   .setBounds(rowB.removeFromLeft(sw));
    resonanceB.setBounds(rowB.removeFromLeft(sw));
    driveB    .setBounds(rowB.removeFromLeft(sw));
    envAmtB   .setBounds(rowB.removeFromLeft(sw));
    lfoAmtB   .setBounds(rowB);
}
