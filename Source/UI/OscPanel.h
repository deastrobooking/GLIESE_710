#pragma once
#include <JuceHeader.h>
#include "IndustrialTheme.h"

// Draws the current VA waveform shape — updated via setShape()
class VAWaveDisplay : public juce::Component
{
public:
    explicit VAWaveDisplay(juce::Colour accentColour) : accent(accentColour) {}

    void setShape(int shapeIndex)
    {
        if (shape == shapeIndex) return;
        shape = shapeIndex;
        repaint();
    }

    void paint(juce::Graphics& g) override;

private:
    int          shape  = 0;
    juce::Colour accent;
};

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
    juce::Colour accent;

    juce::Label titleLabel;

    using Att  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Engine enable / hard shutoff
    juce::ToggleButton enableButton;
    std::unique_ptr<BAtt> enableAtt;

    // Level + octave shared by all osc types
    juce::Slider levelSlider, octaveSlider;
    std::unique_ptr<Att> levelAtt, octaveAtt;

    // ADSR sliders
    juce::Slider adsrA, adsrD, adsrS, adsrR;
    std::unique_ptr<Att> attA, attD, attS, attR;

    juce::ComboBox modeBoxA, modeBoxB;
    std::unique_ptr<CAtt> modeAttA, modeAttB;

    // Waveform preview — only created for VA panels
    std::unique_ptr<VAWaveDisplay> waveDisplay;

    juce::Slider extra1, extra2, extra3, extra4, extra5, extra6, extra7, extra8, extra9, extra10;
    std::unique_ptr<Att> extraAtt1, extraAtt2, extraAtt3, extraAtt4, extraAtt5,
                         extraAtt6, extraAtt7, extraAtt8, extraAtt9, extraAtt10;
    int numExtraSliders = 0;
    int numModeBoxes = 0;

    juce::Label levelLabel, octaveLabel;
    juce::Label modeALabel, modeBLabel;
    std::array<juce::Label, 10> extraLabels;
    std::array<juce::Label,  4> adsrLabels;  // A D S R

    void setupSlider(juce::Slider& slider, const juce::String& tooltip);
    void setupADSRSlider(juce::Slider& slider, const juce::String& tooltip);
    void setupExtraSlider(juce::Slider& slider, const juce::String& paramId,
                          const juce::String& tooltip, std::unique_ptr<Att>& attachment);
};
