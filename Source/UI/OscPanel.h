#pragma once
#include <JuceHeader.h>

// Panel for one oscillator section (3D, FM, WT, or VA)
class OscPanel : public juce::Component
{
public:
    explicit OscPanel(juce::AudioProcessorValueTreeState& apvts, const juce::String& oscId,
                      const juce::String& label);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String oscId;
    juce::String label;

    juce::Label titleLabel;

    // Level + octave shared by all osc types
    juce::Slider levelSlider, octaveSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAtt, octaveAtt;

    // ADSR sliders
    juce::Slider adsrA, adsrD, adsrS, adsrR;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attA, attD, attS, attR;

    juce::Label levelLabel, adsrLabel;
};
