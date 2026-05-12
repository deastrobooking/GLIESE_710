#pragma once
#include <JuceHeader.h>

// Each row = one mod slot: [Source combo] [Dest combo] [Amount knob] [Bipolar toggle]
class ModMatrixPanel : public juce::Component
{
public:
    static constexpr int kNumSlots    = 32;
    static constexpr int kRowHeight   = 28;
    static constexpr int kVisibleRows = 8;

    explicit ModMatrixPanel(juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    struct SlotRow
    {
        juce::ComboBox source, dest;
        juce::Slider   amount;
        juce::ToggleButton bipolar;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAtt, dstAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amtAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   bipAtt;
    };

    std::array<SlotRow, kNumSlots> rows;
    juce::Viewport viewport;
    juce::Component rowContainer;

    juce::Label titleLabel;
};
