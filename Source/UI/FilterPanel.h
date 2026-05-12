#pragma once
#include <JuceHeader.h>

class FilterPanel : public juce::Component
{
public:
    explicit FilterPanel(juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Filter A controls
    juce::ComboBox filterAType, filterBType, filterRouting;
    juce::Slider cutoffA, resonanceA, driveA, envAmtA, lfoAmtA;
    juce::Slider cutoffB, resonanceB, driveB, envAmtB, lfoAmtB;

    using Att   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<CAtt> typeAAtt, typeBAtt, routingAtt;
    std::unique_ptr<Att>  cutAAtt, resAAtt, drvAAtt, envAAtt, lfoAAtt;
    std::unique_ptr<Att>  cutBAtt, resBAtt, drvBAtt, envBAtt, lfoBAtt;

    juce::Label titleLabel;
    std::array<juce::Label, 3> comboLabels;   // TYPE A / TYPE B / ROUTING
    std::array<juce::Label, 5> colLabels;     // CUT / RES / DRIVE / ENV / LFO

    void addSlider(juce::Slider& s, const juce::String& paramId, const juce::String& tip,
                   std::unique_ptr<Att>& att);
};
