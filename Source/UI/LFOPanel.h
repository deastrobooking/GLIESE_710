#pragma once
#include <JuceHeader.h>
#include <array>

class LFOPanel : public juce::Component,
                 private juce::Timer
{
public:
    explicit LFOPanel(juce::AudioProcessorValueTreeState& apvts);
    ~LFOPanel() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    std::array<std::atomic<float>*, 4> syncParams {};
    std::array<bool, 4> syncModes {};
    void updateModeVisibility(int lfoIndex, bool synced);

    juce::AudioProcessorValueTreeState& apvts;

    struct LFOControls
    {
        juce::Label        titleLabel;
        juce::ComboBox     shapeBox;
        juce::Slider       syncSwitch;       // 0 = free, 1 = synced
        juce::Slider       syncRateSlider;   // 0..10, continuous beat-division rate
        juce::Slider       rateSlider;
        juce::Slider       amountSlider;
        juce::Label        shapeLabel, rateLabel, amtLabel, syncRateLabel;
        juce::Label        freeLabel, syncedLabel;
        juce::ToggleButton retrigButton;

        using Att  = juce::AudioProcessorValueTreeState::SliderAttachment;
        using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
        std::unique_ptr<CAtt> shapeAtt;
        std::unique_ptr<Att>  rateAtt, syncAtt, syncRateAtt, amtAtt;
        std::unique_ptr<BAtt> retrigAtt;
    };

    std::array<LFOControls, 4> lfos;
    juce::Label panelTitle;
};
