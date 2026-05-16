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
    juce::ComboBox oversampleA, oversampleB;

    // Main filter params
    juce::Slider cutoffA, resonanceA, driveA, morphA, saturationA, bassCompA, gainA, mixA, envAmtA, lfoAmtA;
    juce::Slider cutoffB, resonanceB, driveB, morphB, saturationB, bassCompB, gainB, mixB, envAmtB, lfoAmtB;

    // Per-filter envelope ADSR — 4 knobs per filter
    juce::Slider adsrAA, adsrDA, adsrSA, adsrRA;
    juce::Slider adsrAB, adsrDB, adsrSB, adsrRB;

    using Att   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<CAtt> typeAAtt, typeBAtt, routingAtt, osAAtt, osBAtt;
    std::unique_ptr<Att>  cutAAtt, resAAtt, drvAAtt, morphAAtt, satAAtt, bassAAtt, gainAAtt, mixAAtt, envAAtt, lfoAAtt;
    std::unique_ptr<Att>  cutBAtt, resBAtt, drvBAtt, morphBAtt, satBAtt, bassBAtt, gainBAtt, mixBAtt, envBAtt, lfoBAtt;
    std::unique_ptr<Att>  adsrAAAtt, adsrDAAtt, adsrSAAtt, adsrRAAtt;
    std::unique_ptr<Att>  adsrABAtt, adsrDBAtt, adsrSBAtt, adsrRBAtt;

    juce::Label titleLabel;
    std::array<juce::Label, 3> comboLabels;   // TYPE A / TYPE B / ROUTING
    std::array<juce::Label, 15> colLabels;    // CUT RES DRV MOR SAT BASS GAIN MIX ENV LFO ATK DEC SUS REL OS

    void addSlider(juce::Slider& s, const juce::String& paramId, const juce::String& tip,
                   std::unique_ptr<Att>& att);
};
