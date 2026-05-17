#pragma once

#include <JuceHeader.h>
#include "../Engine/Modulation/StepSequencerMod.h"
#include <array>
#include <functional>

class ModStepSequencerPanel : public juce::Component,
                              private juce::Timer
{
public:
    using CurrentStepProvider = std::function<int()>;

    ModStepSequencerPanel(juce::AudioProcessorValueTreeState& apvts,
                          CurrentStepProvider currentStepProvider);
    ~ModStepSequencerPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class SolidButton : public juce::TextButton
    {
    public:
        void setSolidColour(juce::Colour colour);
        void paintButton(juce::Graphics&, bool highlighted, bool down) override;

    private:
        juce::Colour solidColour { juce::Colours::white };
    };

    class StepGrid : public juce::Component
    {
    public:
        StepGrid(juce::AudioProcessorValueTreeState& apvts,
                 CurrentStepProvider currentStepProvider);

        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;

        void randomizeLane(int lane);
        void mirrorLane(int lane);
        void shiftLane(int lane, int direction);
        void riseLane(int lane);
        void fallLane(int lane);
        void sweepLane(int lane);
        void upDownLane(int lane);
        void invertLane(int lane);
        void sparseLane(int lane);
        void algoLane(int lane);
        void quantizeLane(int lane);

        void setActiveSegment(int segment);
        int getActiveSegment() const noexcept { return activeSegment; }

    private:
        juce::AudioProcessorValueTreeState& apvts;
        CurrentStepProvider currentStepProvider;
        std::array<std::atomic<float>*, StepSequencerMod::kNumLanes * StepSequencerMod::kNumSteps> stepValues {};
        std::array<juce::RangedAudioParameter*, StepSequencerMod::kNumLanes * StepSequencerMod::kNumSteps> stepParams {};
        std::atomic<float>* lengthValue = nullptr;
        std::atomic<float>* loopLengthValue = nullptr;
        juce::Random rng;
        int activeSegment = 0;

        float getStepValue(int lane, int step) const;
        void setStepValue(int lane, int step, float value);
        void updateFromMouse(juce::Point<int> position);
        juce::Rectangle<float> laneBounds(int lane) const;
        int stepAtX(float x) const;
        int segmentStart() const noexcept;
        int getSequenceLength() const noexcept;
        int getActiveSequenceLength() const noexcept;
        static size_t indexFor(int lane, int step);
    };

    juce::AudioProcessorValueTreeState& apvts;
    CurrentStepProvider currentStepProvider;
    StepGrid grid;

    juce::Label titleLabel;
    juce::Slider depthSlider;
    juce::Slider loopLengthSlider;
    juce::ComboBox divisionBox;
    juce::ComboBox subdivisionBox;
    juce::Label depthLabel, divisionLabel, subdivisionLabel, loopLengthLabel, lengthLabel, segmentLabel;
    std::array<SolidButton, 4> lengthButtons;
    std::array<SolidButton, StepSequencerMod::kNumSegments> segmentButtons;

    std::array<juce::Label, StepSequencerMod::kNumLanes> laneLabels;
    std::array<juce::ComboBox, StepSequencerMod::kNumLanes> targetBoxes;
    std::array<juce::Label, StepSequencerMod::kNumLanes> targetLabels;
    std::array<std::array<SolidButton, 12>, StepSequencerMod::kNumLanes> patternButtons;

    using Att = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<Att> depthAtt;
    std::unique_ptr<Att> loopLengthAtt;
    std::unique_ptr<CAtt> divisionAtt;
    std::unique_ptr<CAtt> subdivisionAtt;
    std::array<std::unique_ptr<CAtt>, StepSequencerMod::kNumLanes> targetAtts;
    juce::RangedAudioParameter* lengthParam = nullptr;
    std::atomic<float>* lengthValue = nullptr;

    void timerCallback() override;
    void applyPattern(int lane, int patternIndex);
    void setSequenceLengthIndex(int index);
    int getSequenceLengthIndex() const noexcept;
    int getSequenceLength() const noexcept;
    void setActiveSegment(int segment);
    void updateLengthAndSegmentButtons();
};
