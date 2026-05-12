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

    using Att = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Level + octave shared by all osc types
    juce::Slider levelSlider, octaveSlider;
    std::unique_ptr<Att> levelAtt, octaveAtt;

    // ADSR sliders
    juce::Slider adsrA, adsrD, adsrS, adsrR;
    std::unique_ptr<Att> attA, attD, attS, attR;

    juce::ComboBox modeBoxA, modeBoxB;
    std::unique_ptr<CAtt> modeAttA, modeAttB;

    juce::Slider extra1, extra2, extra3, extra4, extra5, extra6, extra7, extra8, extra9, extra10;
    std::unique_ptr<Att> extraAtt1, extraAtt2, extraAtt3, extraAtt4, extraAtt5,
                         extraAtt6, extraAtt7, extraAtt8, extraAtt9, extraAtt10;
    int numExtraSliders = 0;
    int numModeBoxes = 0;

    juce::Label levelLabel, adsrLabel;

    void setupSlider(juce::Slider& slider, const juce::String& tooltip);
    void setupExtraSlider(juce::Slider& slider, const juce::String& paramId,
                          const juce::String& tooltip, std::unique_ptr<Att>& attachment);
};
