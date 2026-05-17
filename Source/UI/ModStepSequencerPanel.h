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

    class ResetButton : public juce::TextButton
    {
    public:
        void paintButton(juce::Graphics&, bool highlighted, bool down) override;
    };

    class StepGrid : public juce::Component
    {
    public:
        enum class DrawMode
        {
            Free = 0,
            Ramp,
            Curve,
            Corner
        };

        StepGrid(juce::AudioProcessorValueTreeState& apvts,
                 CurrentStepProvider currentStepProvider);

        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;

        void setDrawMode(DrawMode mode);
        DrawMode getDrawMode() const noexcept { return drawMode; }
        void setHardShapeSteps(bool shouldUseHardSteps);
        bool getHardShapeSteps() const noexcept { return hardShapeSteps; }
        void applyMasterShape(int shapeIndex);

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
        DrawMode drawMode = DrawMode::Free;
        bool hardShapeSteps = false;

        struct DrawGesture
        {
            bool active = false;
            bool draggingHandle = false;
            int lane = 0;
            int startStep = 0;
            float startValue = 0.0f;
        };

        struct EditableShape
        {
            bool active = false;
            DrawMode mode = DrawMode::Ramp;
            int lane = 0;
            int startStep = 0;
            int endStep = 0;
            int cornerStep = 0;
            float startValue = 0.0f;
            float endValue = 0.0f;
            float cornerValue = 0.5f;
        };

        DrawGesture gesture;
        EditableShape editableShape;

        float getStepValue(int lane, int step) const;
        void setStepValue(int lane, int step, float value);
        void updateFromMouse(juce::Point<int> position);
        void beginShapeGesture(juce::Point<int> position);
        void updateShapeGesture(juce::Point<int> position);
        void updateShapeHandle(juce::Point<int> position);
        void applyEditableShape();
        void paintEditableShape(juce::Graphics& g) const;
        bool hitEditableShapeHandle(juce::Point<int> position) const;
        float valueForEditableShapeAtStep(int step) const;
        float defaultStepValue(int lane, int step) const;
        juce::Point<float> pointForStepValue(int lane, int step, float value) const;
        juce::Rectangle<float> laneBounds(int lane) const;
        int laneAtY(float y) const;
        int stepAtX(float x) const;
        float valueAtY(int lane, float y) const;
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
    juce::Slider sequenceLengthSlider;
    ResetButton loopLengthResetButton;
    juce::Slider loopLengthSlider;
    juce::ComboBox divisionBox;
    juce::ComboBox subdivisionBox;
    juce::Label depthLabel, sequenceLengthLabel, divisionLabel, subdivisionLabel, loopLengthLabel, lengthLabel, segmentLabel, drawModeLabel, masterShapeLabel;
    SolidButton hardStepButton;
    std::array<SolidButton, 4> drawModeButtons;
    std::array<SolidButton, 4> masterShapeButtons;
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
    void setDrawMode(StepGrid::DrawMode mode);
    void updateDrawModeButtons();
    void setHardShapeSteps(bool shouldUseHardSteps);
    void updateHardStepButton();
    void applyMasterShape(int shapeIndex);
};
