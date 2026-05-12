#pragma once
#include <JuceHeader.h>

class ArpPanel : public juce::Component
{
public:
    explicit ArpPanel(juce::AudioProcessorValueTreeState& apvts);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    juce::ToggleButton onToggle;
    juce::ComboBox     patternBox, rateBox;
    juce::ToggleButton syncToggle, latchToggle;
    juce::Slider       octavesSlider, gateSlider, swingSlider;
    juce::Label        titleLabel;
    juce::Label        patternLabel, rateLabel;
    juce::Label        octLabel, gateLabel, swingLabel;

    using Att  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<BAtt>  onAtt, syncAtt, latchAtt;
    std::unique_ptr<CAtt>  patternAtt, rateAtt;
    std::unique_ptr<Att>   octAtt, gateAtt, swingAtt;
};
