#include "LFOPanel.h"
#include "IndustrialTheme.h"

static const juce::StringArray kShapes { "Sine","Triangle","Saw","RevSaw","Square","S&H","Smooth S&H" };
static const juce::StringArray kSync   { "Free","1/32","1/16","1/8","1/4","1/2","1 Bar","2 Bars","4 Bars" };

LFOPanel::LFOPanel(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    auto panelAccent = IndustrialTheme::accentLFO(0);
    panelTitle.setText("LFOs", juce::dontSendNotification);
    IndustrialTheme::styleTitle(panelTitle, panelAccent);
    addAndMakeVisible(panelTitle);

    auto makeLabel = [&](juce::Label& l, const juce::String& text, juce::Component& parent) {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, panelAccent, 10.f);
        parent.addAndMakeVisible(l);
    };

    for (int i = 0; i < 4; ++i)
    {
        auto& lfo = lfos[(size_t)i];
        juce::String p = "lfo" + juce::String(i + 1) + "_";
        auto accent = IndustrialTheme::accentLFO(i);

        lfo.titleLabel.setText("LFO " + juce::String(i + 1), juce::dontSendNotification);
        IndustrialTheme::styleTitle(lfo.titleLabel, accent, 11.f);
        lfo.titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lfo.titleLabel);

        lfo.shapeBox.addItemList(kShapes, 1);
        IndustrialTheme::styleCombo(lfo.shapeBox, accent);
        addAndMakeVisible(lfo.shapeBox);
        lfo.shapeAtt = std::make_unique<LFOControls::CAtt>(apvts, p + "shape", lfo.shapeBox);

        lfo.syncBox.addItemList(kSync, 1);
        IndustrialTheme::styleCombo(lfo.syncBox, accent);
        addAndMakeVisible(lfo.syncBox);
        lfo.syncAtt = std::make_unique<LFOControls::CAtt>(apvts, p + "sync", lfo.syncBox);

        lfo.rateSlider.setSliderStyle(juce::Slider::LinearVertical);
        lfo.rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 14);
        lfo.rateSlider.setTooltip("LFO " + juce::String(i + 1) + " Rate (Hz)");
        IndustrialTheme::styleSlider(lfo.rateSlider, accent);
        addAndMakeVisible(lfo.rateSlider);
        lfo.rateAtt = std::make_unique<LFOControls::Att>(apvts, p + "rate", lfo.rateSlider);

        lfo.amountSlider.setSliderStyle(juce::Slider::LinearVertical);
        lfo.amountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 14);
        lfo.amountSlider.setTooltip("LFO " + juce::String(i + 1) + " Amount");
        IndustrialTheme::styleSlider(lfo.amountSlider, accent);
        addAndMakeVisible(lfo.amountSlider);
        lfo.amtAtt = std::make_unique<LFOControls::Att>(apvts, p + "amount", lfo.amountSlider);

        lfo.retrigButton.setButtonText("RETRIG");
        IndustrialTheme::styleButton(lfo.retrigButton, accent);
        addAndMakeVisible(lfo.retrigButton);
        lfo.retrigAtt = std::make_unique<LFOControls::BAtt>(apvts, p + "retrig", lfo.retrigButton);

        makeLabel(lfo.shapeLabel, "SHAPE", *this);
        makeLabel(lfo.rateLabel,  "RATE",  *this);
        makeLabel(lfo.amtLabel,   "AMT",   *this);
        makeLabel(lfo.syncLabel,  "SYNC",  *this);
    }
}

void LFOPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintPanel(g, getLocalBounds(), IndustrialTheme::accentLFO(0), true);

    // Vertical dividers between LFO columns
    int w = getWidth() - 12;
    for (int i = 1; i < 4; ++i)
    {
        int x = 6 + w * i / 4;
        g.setColour(IndustrialTheme::accentLFO(i).withAlpha(0.35f));
        g.drawLine((float)x, 6.f, (float)x, (float)(getHeight() - 6), 1.f);
    }
}

void LFOPanel::resized()
{
    auto area = getLocalBounds().reduced(6);
    panelTitle.setBounds(area.removeFromTop(18));
    area.removeFromTop(2);

    int colW = area.getWidth() / 4;

    for (int i = 0; i < 4; ++i)
    {
        auto& lfo = lfos[(size_t)i];
        auto col  = area.withWidth(colW).withX(area.getX() + colW * i).reduced(3, 0);

        lfo.titleLabel.setBounds(col.removeFromTop(14));

        lfo.shapeLabel.setBounds(col.removeFromTop(11));
        lfo.shapeBox  .setBounds(col.removeFromTop(20));
        col.removeFromTop(2);

        lfo.syncLabel.setBounds(col.removeFromTop(11));
        lfo.syncBox  .setBounds(col.removeFromTop(20));
        col.removeFromTop(2);

        // Rate and Amount knobs side by side
        auto knobRow   = col;
        int  halfW     = knobRow.getWidth() / 2;
        auto rateCol   = knobRow.withWidth(halfW);
        auto amtCol    = knobRow.withX(knobRow.getX() + halfW).withWidth(halfW);

        lfo.rateLabel  .setBounds(rateCol.removeFromTop(11));
        lfo.rateSlider .setBounds(rateCol);
        lfo.amtLabel   .setBounds(amtCol .removeFromTop(11));
        lfo.amountSlider.setBounds(amtCol);
    }
}
