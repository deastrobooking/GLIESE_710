#pragma once
#include <JuceHeader.h>
#include <array>

class LFOPanel : public juce::Component
{
public:
    explicit LFOPanel(juce::AudioProcessorValueTreeState& apvts);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    struct LFOControls
    {
        juce::Label        titleLabel;
        juce::ComboBox     shapeBox;
        juce::ComboBox     syncBox;
        juce::Slider       rateSlider;
        juce::Slider       amountSlider;
        juce::Label        shapeLabel, rateLabel, amtLabel, syncLabel;
        juce::ToggleButton retrigButton;

        using Att  = juce::AudioProcessorValueTreeState::SliderAttachment;
        using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
        std::unique_ptr<CAtt> shapeAtt, syncAtt;
        std::unique_ptr<Att>  rateAtt, amtAtt;
        std::unique_ptr<BAtt> retrigAtt;
    };

    std::array<LFOControls, 4> lfos;
    juce::Label panelTitle;
};
