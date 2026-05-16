#include "ModMatrixPanel.h"
#include "IndustrialTheme.h"
#include "../Engine/Modulation/ModNames.h"

ModMatrixPanel::ModMatrixPanel(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    auto accent = IndustrialTheme::accentMod();
    titleLabel.setText("MOD MATRIX", juce::dontSendNotification);
    IndustrialTheme::styleTitle(titleLabel, accent);
    addAndMakeVisible(titleLabel);

    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& row = rows[(size_t) i];
        juce::String pfx = "mod" + juce::String(i) + "_";

        row.source.addItemList(ModNames::sourceDisplayNames(), 1);
        row.source.setSelectedId(1, juce::dontSendNotification);
        IndustrialTheme::styleCombo(row.source, accent);
        rowContainer.addAndMakeVisible(row.source);
        row.srcAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, pfx + "source", row.source);

        row.dest.addItemList(ModNames::destDisplayNames(), 1);
        row.dest.setSelectedId(1, juce::dontSendNotification);
        IndustrialTheme::styleCombo(row.dest, accent);
        rowContainer.addAndMakeVisible(row.dest);
        row.dstAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, pfx + "dest", row.dest);

        row.amount.setRange(-1.0, 1.0);
        row.amount.setSliderStyle(juce::Slider::LinearHorizontal);
        row.amount.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, kRowHeight - 4);
        IndustrialTheme::styleSlider(row.amount, accent);
        rowContainer.addAndMakeVisible(row.amount);
        row.amtAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, pfx + "amount", row.amount);

        row.bipolar.setButtonText("~");
        row.bipolar.setTooltip("Bipolar");
        IndustrialTheme::styleButton(row.bipolar, accent);
        rowContainer.addAndMakeVisible(row.bipolar);
        row.bipAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, pfx + "bipolar", row.bipolar);
    }

    viewport.setViewedComponent(&rowContainer, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    auto makeHeader = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, accent, 10.f, juce::Justification::centredLeft);
        addAndMakeVisible(l);
    };

    makeHeader(headerSource,  "SOURCE");
    makeHeader(headerDest,    "DESTINATION");
    makeHeader(headerAmount,  "AMOUNT");
    makeHeader(headerBipolar, "~");
}

void ModMatrixPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintPanel(g, getLocalBounds(), IndustrialTheme::accentMod(), true);
}

void ModMatrixPanel::resized()
{
    auto area = getLocalBounds().reduced(6);
    titleLabel.setBounds(area.removeFromTop(20));

    // Column header row — mirrors the slot row geometry
    auto hdr = area.removeFromTop(16);
    int w = area.getWidth();
    int x = 0;
    headerSource .setBounds(hdr.withWidth(120).withX(hdr.getX() + x)); x += 122;
    headerDest   .setBounds(hdr.withWidth(120).withX(hdr.getX() + x)); x += 122;
    headerAmount .setBounds(hdr.withWidth(w - x - 30).withX(hdr.getX() + x)); x += w - x - 28;
    headerBipolar.setBounds(hdr.withWidth(24).withX(hdr.getX() + x));

    viewport.setBounds(area);

    int containerH = kNumSlots * kRowHeight;
    rowContainer.setBounds(0, 0, area.getWidth(), containerH);

    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& row = rows[(size_t) i];
        juce::Rectangle<int> r(0, i * kRowHeight, w, kRowHeight - 2);
        x = 0;
        row.source.setBounds(r.withWidth(120).withX(x)); x += 122;
        row.dest  .setBounds(r.withWidth(120).withX(x)); x += 122;
        row.amount.setBounds(r.withWidth(w - x - 30).withX(x)); x += w - x - 28;
        row.bipolar.setBounds(r.withWidth(24).withX(x));
    }
}
