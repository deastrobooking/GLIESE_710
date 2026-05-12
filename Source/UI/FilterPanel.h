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

    juce::ComboBox filterAType, filterBType, filterRouting;

    // Main filter params — 5 knobs per filter
    juce::Slider cutoffA, resonanceA, driveA, envAmtA, lfoAmtA;
    juce::Slider cutoffB, resonanceB, driveB, envAmtB, lfoAmtB;

    // Per-filter envelope ADSR — 4 knobs per filter
    juce::Slider adsrAA, adsrDA, adsrSA, adsrRA;
    juce::Slider adsrAB, adsrDB, adsrSB, adsrRB;

    using Att   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<CAtt> typeAAtt, typeBAtt, routingAtt;
    std::unique_ptr<Att>  cutAAtt, resAAtt, drvAAtt, envAAtt, lfoAAtt;
    std::unique_ptr<Att>  cutBAtt, resBAtt, drvBAtt, envBAtt, lfoBAtt;
    std::unique_ptr<Att>  adsrAAAtt, adsrDAAtt, adsrSAAtt, adsrRAAtt;
    std::unique_ptr<Att>  adsrABAtt, adsrDBAtt, adsrSBAtt, adsrRBAtt;

    juce::Label titleLabel;
    std::array<juce::Label, 3> comboLabels;   // TYPE A / TYPE B / ROUTING
    std::array<juce::Label, 9> colLabels;     // CUT RES DRV ENV LFO | ATK DEC SUS REL

    void addSlider(juce::Slider& s, const juce::String& paramId, const juce::String& tip,
                   std::unique_ptr<Att>& att);
};
