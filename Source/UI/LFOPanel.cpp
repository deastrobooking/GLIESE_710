#include "LFOPanel.h"
#include "IndustrialTheme.h"

static const juce::StringArray kShapes { "Sine","Triangle","Saw","RevSaw","Square","S&H","Smooth S&H" };

// Beat-division labels matching LFOEngine::kSyncBeats (index 0..10)
static const char* kSyncLabels[] = {
    "8 Bars","4 Bars","2 Bars","1 Bar","1/2","1/4","1/8","1/16","1/32","1/64","1/128"
};

LFOPanel::LFOPanel(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    auto panelAccent = IndustrialTheme::accentLFO(0);
    panelTitle.setText("LFOs", juce::dontSendNotification);
    IndustrialTheme::styleTitle(panelTitle, panelAccent);
    addAndMakeVisible(panelTitle);

    for (int i = 0; i < 4; ++i)
    {
        auto& lfo = lfos[(size_t)i];
        juce::String p = "lfo" + juce::String(i + 1) + "_";
        auto accent = IndustrialTheme::accentLFO(i);

        // ── Title ─────────────────────────────────────────────────────────────
        lfo.titleLabel.setText("LFO " + juce::String(i + 1), juce::dontSendNotification);
        IndustrialTheme::styleTitle(lfo.titleLabel, accent, 11.f);
        lfo.titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lfo.titleLabel);

        // ── Shape combo ───────────────────────────────────────────────────────
        lfo.shapeBox.addItemList(kShapes, 1);
        IndustrialTheme::styleCombo(lfo.shapeBox, accent);
        addAndMakeVisible(lfo.shapeBox);
        lfo.shapeAtt = std::make_unique<LFOControls::CAtt>(apvts, p + "shape", lfo.shapeBox);

        // ── FREE / SYNCED toggle switch ────────────────────────────────────────
        // Horizontal snap slider 0=FREE, 1=SYNCED, styled as a pill switch.
        lfo.syncSwitch.setSliderStyle(juce::Slider::LinearHorizontal);
        lfo.syncSwitch.setRange(0.0, 1.0, 1.0);
        lfo.syncSwitch.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        lfo.syncSwitch.setTooltip("Switch between free-running and BPM-synced mode");
        IndustrialTheme::styleSlider(lfo.syncSwitch, accent);
        addAndMakeVisible(lfo.syncSwitch);
        lfo.syncAtt = std::make_unique<LFOControls::Att>(apvts, p + "sync", lfo.syncSwitch);

        lfo.freeLabel.setText("FREE", juce::dontSendNotification);
        IndustrialTheme::styleLabel(lfo.freeLabel, accent, 9.f, juce::Justification::centredRight);
        addAndMakeVisible(lfo.freeLabel);

        lfo.syncedLabel.setText("SYNCD", juce::dontSendNotification);
        IndustrialTheme::styleLabel(lfo.syncedLabel, accent, 9.f, juce::Justification::centredLeft);
        addAndMakeVisible(lfo.syncedLabel);

        // ── Free rate slider (Hz) ─────────────────────────────────────────────
        lfo.rateSlider.setSliderStyle(juce::Slider::LinearVertical);
        lfo.rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 14);
        lfo.rateSlider.setTooltip("LFO " + juce::String(i + 1) + " Rate (Hz)");
        IndustrialTheme::styleSlider(lfo.rateSlider, accent);
        addAndMakeVisible(lfo.rateSlider);
        lfo.rateAtt = std::make_unique<LFOControls::Att>(apvts, p + "rate", lfo.rateSlider);

        // ── Sync rate slider (beat divisions, 0=8Bars .. 10=1/128) ────────────
        lfo.syncRateSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        lfo.syncRateSlider.setRange(0.0, 10.0, 0.001);
        lfo.syncRateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
        lfo.syncRateSlider.setTooltip("BPM-synced rate: 8 Bars (slowest) to 1/128 (fastest). Automatable.");
        lfo.syncRateSlider.textFromValueFunction = [](double v) -> juce::String {
            int idx = juce::roundToInt(juce::jlimit(0.0, 10.0, v));
            return juce::String(kSyncLabels[idx]);
        };
        IndustrialTheme::styleSlider(lfo.syncRateSlider, accent);
        addAndMakeVisible(lfo.syncRateSlider);
        lfo.syncRateAtt = std::make_unique<LFOControls::Att>(apvts, p + "sync_rate", lfo.syncRateSlider);

        // ── Amount slider ─────────────────────────────────────────────────────
        lfo.amountSlider.setSliderStyle(juce::Slider::LinearVertical);
        lfo.amountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 44, 14);
        lfo.amountSlider.setTooltip("LFO " + juce::String(i + 1) + " Amount");
        IndustrialTheme::styleSlider(lfo.amountSlider, accent);
        addAndMakeVisible(lfo.amountSlider);
        lfo.amtAtt = std::make_unique<LFOControls::Att>(apvts, p + "amount", lfo.amountSlider);

        // ── Labels ────────────────────────────────────────────────────────────
        auto makeLabel = [&](juce::Label& l, const juce::String& text) {
            l.setText(text, juce::dontSendNotification);
            IndustrialTheme::styleLabel(l, accent, 10.f);
            addAndMakeVisible(l);
        };
        makeLabel(lfo.shapeLabel,    "SHAPE");
        makeLabel(lfo.rateLabel,     "RATE");
        makeLabel(lfo.syncRateLabel, "SYNC RATE");
        makeLabel(lfo.amtLabel,      "AMT");

        // ── Retrig toggle ─────────────────────────────────────────────────────
        lfo.retrigButton.setButtonText("RETRIG");
        IndustrialTheme::styleButton(lfo.retrigButton, accent);
        addAndMakeVisible(lfo.retrigButton);
        lfo.retrigAtt = std::make_unique<LFOControls::BAtt>(apvts, p + "retrig", lfo.retrigButton);

        // ── Initial visibility ────────────────────────────────────────────────
        bool synced = apvts.getRawParameterValue(p + "sync")->load() > 0.5f;
        updateModeVisibility(i, synced);

        // Poll atomics on the message thread, including changes from host automation.
        syncParams[(size_t)i] = apvts.getRawParameterValue(p + "sync");
        syncModes[(size_t)i] = synced;
    }
    startTimerHz(30);
}

LFOPanel::~LFOPanel()
{
    stopTimer();
}

void LFOPanel::timerCallback()
{
    bool changed = false;
    for (size_t i = 0; i < syncParams.size(); ++i)
    {
        const bool synced = syncParams[i]->load(std::memory_order_relaxed) > 0.5f;
        if (synced != syncModes[i])
        {
            syncModes[i] = synced;
            updateModeVisibility((int)i, synced);
            changed = true;
        }
    }
    if (changed)
        resized();
}

void LFOPanel::updateModeVisibility(int lfoIndex, bool synced)
{
    auto& lfo = lfos[(size_t)lfoIndex];
    lfo.rateSlider    .setVisible(!synced);
    lfo.rateLabel     .setVisible(!synced);
    lfo.syncRateSlider.setVisible(synced);
    lfo.syncRateLabel .setVisible(synced);
}

void LFOPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintPanel(g, getLocalBounds(), IndustrialTheme::accentLFO(0), true);

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

        // Title
        lfo.titleLabel.setBounds(col.removeFromTop(14));
        col.removeFromTop(2);

        // Shape
        lfo.shapeLabel.setBounds(col.removeFromTop(11));
        lfo.shapeBox  .setBounds(col.removeFromTop(20));
        col.removeFromTop(4);

        // FREE [switch] SYNCED row
        {
            auto switchRow = col.removeFromTop(16);
            int labW = switchRow.getWidth() / 4;
            lfo.freeLabel  .setBounds(switchRow.removeFromLeft(labW));
            lfo.syncedLabel.setBounds(switchRow.removeFromRight(labW));
            lfo.syncSwitch .setBounds(switchRow);
        }
        col.removeFromTop(4);

        // Rate controls: either free-rate or sync-rate (only one visible at a time)
        bool synced = apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "_sync")->load() > 0.5f;

        if (synced)
        {
            // Sync rate: full-width horizontal slider with beat-division label
            lfo.syncRateLabel .setBounds(col.removeFromTop(11));
            lfo.syncRateSlider.setBounds(col.removeFromTop(36));
            col.removeFromTop(4);
        }
        else
        {
            // Free rate: left half, vertical; amount takes right half
            // (both placed here so they overlap the same region)
            lfo.rateLabel .setBounds(col.removeFromTop(11));
            // rateSlider fills left half of the remainder — laid out with amount below
        }

        // Amount always present
        {
            auto knobArea = col;
            if (!synced)
            {
                // Side-by-side: rate left, amount right
                int halfW = knobArea.getWidth() / 2;
                auto rateCol = knobArea.withWidth(halfW);
                auto amtCol  = knobArea.withX(knobArea.getX() + halfW).withWidth(halfW);
                // rateLabel was already removed above; lay out just the slider
                lfo.rateSlider    .setBounds(rateCol);
                lfo.amtLabel      .setBounds(amtCol.removeFromTop(11));
                lfo.amountSlider  .setBounds(amtCol);
            }
            else
            {
                // Full-width amount below the sync-rate slider
                lfo.amtLabel     .setBounds(knobArea.removeFromTop(11));
                lfo.amountSlider .setBounds(knobArea);
            }
        }
    }
}
