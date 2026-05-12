#include "ModMatrixPanel.h"

static const juce::StringArray kSourceNames {
    "None",
    "LFO1","LFO2","LFO3","LFO4",
    "Env Master","Env 3D","Env FM","Env WT","Env VA","Env Filt A","Env Filt B",
    "Velocity","Aftertouch","Mod Wheel","Pitch Bend","CC74","Chan Press",
    "Macro1","Macro2","Macro3","Macro4","Macro5","Macro6","Macro7","Macro8"
};

static const juce::StringArray kDestNames {
    "None",
    "Pitch All",
    "3D Radius","3D Depth","3D Warp","3D Chaos","3D Morph","3D Level",
    "FM Op0 Lvl","FM Op1 Lvl","FM Op2 Lvl","FM Op3 Lvl","FM Op4 Lvl",
    "FM Op5 Lvl","FM Op6 Lvl","FM Op7 Lvl","FM Op8 Lvl","FM Op9 Lvl",
    "FM Op0 Rat","FM Op1 Rat","FM Op2 Rat","FM Op3 Rat","FM Op4 Rat",
    "WT Position","WT Level",
    "VA PW","VA Detune","VA Spread","VA Level",
    "Filt A Cut","Filt A Res","Filt A Drv",
    "Filt B Cut","Filt B Res","Filt B Drv",
    "LFO1 Rate","LFO2 Rate","LFO3 Rate","LFO4 Rate",
    "LFO1 Amt","LFO2 Amt","LFO3 Amt","LFO4 Amt",
    "Master Vol","Master Pan",
    "Arp Gate","Arp Rate",
    "3D FieldFreq","3D Z Ratio","3D Fold","3D Drive"
};

ModMatrixPanel::ModMatrixPanel(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    titleLabel.setText("MOD MATRIX", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont(juce::Font(juce::FontOptions { 13.f, juce::Font::bold }));
    addAndMakeVisible(titleLabel);

    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& row = rows[(size_t) i];
        juce::String pfx = "mod" + juce::String(i) + "_";

        row.source.addItemList(kSourceNames, 1);
        row.source.setSelectedId(1, juce::dontSendNotification);
        rowContainer.addAndMakeVisible(row.source);
        row.srcAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, pfx + "source", row.source);

        row.dest.addItemList(kDestNames, 1);
        row.dest.setSelectedId(1, juce::dontSendNotification);
        rowContainer.addAndMakeVisible(row.dest);
        row.dstAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, pfx + "dest", row.dest);

        row.amount.setRange(-1.0, 1.0);
        row.amount.setSliderStyle(juce::Slider::LinearHorizontal);
        row.amount.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, kRowHeight - 4);
        rowContainer.addAndMakeVisible(row.amount);
        row.amtAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, pfx + "amount", row.amount);

        row.bipolar.setButtonText("~");
        row.bipolar.setTooltip("Bipolar");
        rowContainer.addAndMakeVisible(row.bipolar);
        row.bipAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, pfx + "bipolar", row.bipolar);
    }

    viewport.setViewedComponent(&rowContainer, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    auto makeHeader = [&](juce::Label& l, const juce::String& text) {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colour(0xffcc2222));
        l.setFont(juce::Font(juce::FontOptions { 10.f, juce::Font::bold }));
        l.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(l);
    };

    makeHeader(headerSource,  "SOURCE");
    makeHeader(headerDest,    "DESTINATION");
    makeHeader(headerAmount,  "AMOUNT");
    makeHeader(headerBipolar, "~");
}

void ModMatrixPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff141414));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.f);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.f, 1.f);
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
