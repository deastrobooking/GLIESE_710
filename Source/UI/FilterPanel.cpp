#include "FilterPanel.h"

FilterPanel::FilterPanel(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    titleLabel.setText("DUAL FILTER", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::lightcyan);
    titleLabel.setFont(juce::Font(juce::FontOptions { 13.f, juce::Font::bold }));
    addAndMakeVisible(titleLabel);

    auto setupCombo = [&](juce::ComboBox& cb) {
        cb.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(cb);
    };

    const juce::StringArray filterModes {
        "LP12","LP24","HP12","HP24","Bandpass","Notch",
        "Peak","LoShelf","HiShelf","Comb","AM+","AM-"
    };
    const juce::StringArray routings { "Series","Parallel","A Only","B Only" };

    setupCombo(filterAType);
    filterAType.addItemList(filterModes, 1);
    typeAAtt = std::make_unique<CAtt>(apvts, "filter_a_type", filterAType);

    setupCombo(filterBType);
    filterBType.addItemList(filterModes, 1);
    typeBAtt = std::make_unique<CAtt>(apvts, "filter_b_type", filterBType);

    setupCombo(filterRouting);
    filterRouting.addItemList(routings, 1);
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

    auto makeLabel = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colour(0xff9999bb));
        l.setFont(juce::Font(juce::FontOptions { 10.f }));
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
    };

    makeLabel(comboLabels[0], "TYPE A");
    makeLabel(comboLabels[1], "TYPE B");
    makeLabel(comboLabels[2], "ROUTING");
    makeLabel(colLabels[0], "CUT");
    makeLabel(colLabels[1], "RES");
    makeLabel(colLabels[2], "DRIVE");
    makeLabel(colLabels[3], "ENV");
    makeLabel(colLabels[4], "LFO");
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
    auto portion = [](juce::Rectangle<int> r, int i, int n) {
        int x  = r.getX() + r.getWidth() * i / n;
        int x2 = r.getX() + r.getWidth() * (i + 1) / n;
        return r.withX(x).withWidth(x2 - x);
    };

    auto area = getLocalBounds().reduced(6);
    titleLabel.setBounds(area.removeFromTop(20));

    auto comboLabelRow = area.removeFromTop(13);
    for (int i = 0; i < 3; ++i)
        comboLabels[(size_t)i].setBounds(portion(comboLabelRow, i, 3));

    auto topRow = area.removeFromTop(24);
    filterAType  .setBounds(portion(topRow, 0, 3));
    filterBType  .setBounds(portion(topRow, 1, 3));
    filterRouting.setBounds(portion(topRow, 2, 3));

    area.removeFromTop(4);

    auto colLabelRow = area.removeFromTop(13);
    for (int i = 0; i < 5; ++i)
        colLabels[(size_t)i].setBounds(portion(colLabelRow, i, 5));

    auto rowA = area.removeFromTop(76);
    area.removeFromTop(4);
    auto rowB = area;

    for (int i = 0; i < 5; ++i)
    {
        juce::Slider* sa[5] = { &cutoffA, &resonanceA, &driveA, &envAmtA, &lfoAmtA };
        juce::Slider* sb[5] = { &cutoffB, &resonanceB, &driveB, &envAmtB, &lfoAmtB };
        sa[i]->setBounds(portion(rowA, i, 5));
        sb[i]->setBounds(portion(rowB, i, 5));
    }
}
