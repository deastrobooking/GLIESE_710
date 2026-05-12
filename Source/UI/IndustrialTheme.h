#pragma once
#include <JuceHeader.h>

namespace IndustrialTheme
{
juce::Colour background();
juce::Colour panelBase();
juce::Colour panelEdge();
juce::Colour text();
juce::Colour mutedText();
juce::Colour accentMaster();
juce::Colour accentFM();
juce::Colour accentFilter();
juce::Colour accentMod();
juce::Colour accentArp();
juce::Colour accentLFO(int index);
juce::Colour accentForOsc(const juce::String& oscId);

void styleTitle(juce::Label& label, juce::Colour accent, float size = 13.f);
void styleLabel(juce::Label& label, juce::Colour accent, float size = 10.f,
                juce::Justification justification = juce::Justification::centred);
void styleSlider(juce::Slider& slider, juce::Colour accent);
void styleCombo(juce::ComboBox& combo, juce::Colour accent);
void styleButton(juce::Button& button, juce::Colour accent);

void paintFaceplate(juce::Graphics& g, juce::Rectangle<int> bounds);
void paintPanel(juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour accent,
                bool denseLines = false);

class IndustrialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    IndustrialLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;

    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};
}
