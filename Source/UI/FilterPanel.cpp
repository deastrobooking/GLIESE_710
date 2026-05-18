#include "FilterPanel.h"
#include "IndustrialTheme.h"

FilterPanel::FilterPanel(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    auto accent = IndustrialTheme::accentFilter();
    titleLabel.setText("DUAL FILTER", juce::dontSendNotification);
    IndustrialTheme::styleTitle(titleLabel, accent);
    addAndMakeVisible(titleLabel);

    auto setupCombo = [&](juce::ComboBox& cb) {
        IndustrialTheme::styleCombo(cb, accent);
        addAndMakeVisible(cb);
    };

    const juce::StringArray filterModes {
        "LP12","LP24","HP12","HP24","Bandpass","Notch",
        "Peak","LoShelf","HiShelf","Comb","AM+","AM-",
        "Ladder12","Ladder18","Ladder24","SEM Morph","MS Dual","Formant",
        "Juno-106","Juno-60","Jupiter-4","Jupiter-8","TB-303",
        "P5 Rev2 SSM","P5 Rev3 CEM","Prophet-600","Prophet-6","Pro-One",
        "GLIESE Prism","Alt Ladder","Phys Ladder"
    };
    const juce::StringArray routings { "Series","Parallel","A Only","B Only" };
    const juce::StringArray oversampling { "Off", "2x", "4x" };

    setupCombo(filterAType);
    filterAType.addItemList(filterModes, 1);
    typeAAtt = std::make_unique<CAtt>(apvts, "filter_a_type", filterAType);

    setupCombo(filterBType);
    filterBType.addItemList(filterModes, 1);
    typeBAtt = std::make_unique<CAtt>(apvts, "filter_b_type", filterBType);

    setupCombo(filterRouting);
    filterRouting.addItemList(routings, 1);
    routingAtt = std::make_unique<CAtt>(apvts, "filter_routing", filterRouting);

    setupCombo(oversampleA);
    oversampleA.addItemList(oversampling, 1);
    osAAtt = std::make_unique<CAtt>(apvts, "filter_a_oversample", oversampleA);

    setupCombo(oversampleB);
    oversampleB.addItemList(oversampling, 1);
    osBAtt = std::make_unique<CAtt>(apvts, "filter_b_oversample", oversampleB);

    // ── Filter A main params ──────────────────────────────────────────────────
    addSlider(cutoffA,    "filter_a_cutoff",    "Filter A Cutoff",    cutAAtt);
    addSlider(resonanceA, "filter_a_resonance", "Filter A Resonance", resAAtt);
    addSlider(driveA,     "filter_a_drive",     "Filter A Drive",     drvAAtt);
    addSlider(morphA,     "filter_a_morph",     "Filter A Morph",     morphAAtt);
    addSlider(saturationA,"filter_a_saturation","Filter A Saturation",satAAtt);
    addSlider(bassCompA,  "filter_a_bass_comp", "Filter A Bass Comp", bassAAtt);
    addSlider(gainA,      "filter_a_gain",      "Filter A Output Gain", gainAAtt);
    addSlider(mixA,       "filter_a_mix",       "Filter A Mix",       mixAAtt);
    addSlider(envAmtA,    "filter_a_env_amt",   "Filter A Env Amt",   envAAtt);
    addSlider(lfoAmtA,    "filter_a_lfo_amt",   "Filter A LFO Amt",   lfoAAtt);

    // ── Filter A envelope ADSR ────────────────────────────────────────────────
    addSlider(adsrAA, "filter_a_adsr_a", "Filter A Env Attack",  adsrAAAtt);
    addSlider(adsrDA, "filter_a_adsr_d", "Filter A Env Decay",   adsrDAAtt);
    addSlider(adsrSA, "filter_a_adsr_s", "Filter A Env Sustain", adsrSAAtt);
    addSlider(adsrRA, "filter_a_adsr_r", "Filter A Env Release", adsrRAAtt);

    // ── Filter B main params ──────────────────────────────────────────────────
    addSlider(cutoffB,    "filter_b_cutoff",    "Filter B Cutoff",    cutBAtt);
    addSlider(resonanceB, "filter_b_resonance", "Filter B Resonance", resBAtt);
    addSlider(driveB,     "filter_b_drive",     "Filter B Drive",     drvBAtt);
    addSlider(morphB,     "filter_b_morph",     "Filter B Morph",     morphBAtt);
    addSlider(saturationB,"filter_b_saturation","Filter B Saturation",satBAtt);
    addSlider(bassCompB,  "filter_b_bass_comp", "Filter B Bass Comp", bassBAtt);
    addSlider(gainB,      "filter_b_gain",      "Filter B Output Gain", gainBAtt);
    addSlider(mixB,       "filter_b_mix",       "Filter B Mix",       mixBAtt);
    addSlider(envAmtB,    "filter_b_env_amt",   "Filter B Env Amt",   envBAtt);
    addSlider(lfoAmtB,    "filter_b_lfo_amt",   "Filter B LFO Amt",   lfoBAtt);

    // ── Filter B envelope ADSR ────────────────────────────────────────────────
    addSlider(adsrAB, "filter_b_adsr_a", "Filter B Env Attack",  adsrABAtt);
    addSlider(adsrDB, "filter_b_adsr_d", "Filter B Env Decay",   adsrDBAtt);
    addSlider(adsrSB, "filter_b_adsr_s", "Filter B Env Sustain", adsrSBAtt);
    addSlider(adsrRB, "filter_b_adsr_r", "Filter B Env Release", adsrRBAtt);

    // ── Labels ────────────────────────────────────────────────────────────────
    auto makeLabel = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, accent, 10.f);
        addAndMakeVisible(l);
    };

    makeLabel(comboLabels[0], "TYPE A");
    makeLabel(comboLabels[1], "TYPE B");
    makeLabel(comboLabels[2], "ROUTING");

    // Column headers: filter tone + envelope + quality.
    makeLabel(colLabels[0], "CUT");
    makeLabel(colLabels[1], "RES");
    makeLabel(colLabels[2], "DRV");
    makeLabel(colLabels[3], "MOR");
    makeLabel(colLabels[4], "SAT");
    makeLabel(colLabels[5], "BASS");
    makeLabel(colLabels[6], "GAIN");
    makeLabel(colLabels[7], "MIX");
    makeLabel(colLabels[8], "ENV");
    makeLabel(colLabels[9], "LFO");
    makeLabel(colLabels[10], "ATK");
    makeLabel(colLabels[11], "DEC");
    makeLabel(colLabels[12], "SUS");
    makeLabel(colLabels[13], "REL");
    makeLabel(colLabels[14], "OS");
}

void FilterPanel::addSlider(juce::Slider& s, const juce::String& paramId,
                             const juce::String& tip, std::unique_ptr<Att>& att)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 42, 13);
    s.setTooltip(tip);
    IndustrialTheme::styleSlider(s, IndustrialTheme::accentFilter());
    addAndMakeVisible(s);
    att = std::make_unique<Att>(apvts, paramId, s);
}

void FilterPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintPanel(g, getLocalBounds(), IndustrialTheme::accentFilter(), true);

    // Vertical divider before the ADSR group.
    auto accent = IndustrialTheme::accentFilter();
    auto inner  = getLocalBounds().reduced(6);
    // Skip past title + combos + col-label rows to start of knob area
    int knobTop = inner.getY() + 20 + 13 + 24 + 4 + 13;
    int divX    = inner.getX() + inner.getWidth() * 10 / 15;
    g.setColour(accent.withAlpha(0.4f));
    g.drawLine((float)divX, (float)knobTop, (float)divX, (float)(inner.getBottom()), 1.f);
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

    // 15 shared column headers.
    auto colLabelRow = area.removeFromTop(13);
    for (int i = 0; i < 15; ++i)
        colLabels[(size_t)i].setBounds(portion(colLabelRow, i, 15));

    // Split remaining space equally between Filter A and Filter B rows
    int rowH = (area.getHeight() - 4) / 2;
    auto rowA = area.removeFromTop(rowH);
    area.removeFromTop(4);
    auto rowB = area;

    juce::Component* rowAComponents[15] = { &cutoffA, &resonanceA, &driveA, &morphA, &saturationA,
                                            &bassCompA, &gainA, &mixA, &envAmtA, &lfoAmtA,
                                            &adsrAA, &adsrDA, &adsrSA, &adsrRA, &oversampleA };
    juce::Component* rowBComponents[15] = { &cutoffB, &resonanceB, &driveB, &morphB, &saturationB,
                                            &bassCompB, &gainB, &mixB, &envAmtB, &lfoAmtB,
                                            &adsrAB, &adsrDB, &adsrSB, &adsrRB, &oversampleB };

    for (int i = 0; i < 15; ++i)
    {
        auto aBounds = portion(rowA, i, 15).reduced(1, 0);
        auto bBounds = portion(rowB, i, 15).reduced(1, 0);

        if (i == 14)
        {
            aBounds = aBounds.withHeight(24).withCentre(aBounds.getCentre());
            bBounds = bBounds.withHeight(24).withCentre(bBounds.getCentre());
        }

        rowAComponents[i]->setBounds(aBounds);
        rowBComponents[i]->setBounds(bBounds);
    }
}
