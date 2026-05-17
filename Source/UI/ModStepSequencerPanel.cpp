#include "ModStepSequencerPanel.h"
#include "IndustrialTheme.h"
#include "../Engine/Modulation/ModNames.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kPatternCount = 12;

const juce::StringArray kPatternLabels {
    "RAND", "MIRR", "SHF L", "SHF R",
    "RISE", "FALL", "SINE", "ALT",
    "INV", "SPRS", "WALK", "QNT"
};

const juce::StringArray kPatternTooltips {
    "Randomize steps", "Mirror segment", "Shift segment left", "Shift segment right",
    "Rising ramp", "Falling ramp", "Sine sweep", "Alternating high-low",
    "Invert values", "Sparse random accents", "Random walk", "Quantize values"
};

const juce::Colour kPatternColours[] = {
    juce::Colour(0xff00e5ff), juce::Colour(0xffffd23f), juce::Colour(0xffff7a18), juce::Colour(0xffff2bd6),
    juce::Colour(0xff39ff14), juce::Colour(0xff7aa6ff), juce::Colour(0xffb06cff), juce::Colour(0xfffff000),
    juce::Colour(0xffff3b2f), juce::Colour(0xff00ffcc), juce::Colour(0xffc7ff38), juce::Colour(0xffff9f1c)
};

juce::Colour laneAccent(int lane)
{
    static const juce::Colour accents[] = {
        juce::Colour(0xff00e5ff),
        juce::Colour(0xffffd23f),
        juce::Colour(0xff39ff14),
        juce::Colour(0xffff2bd6)
    };

    return accents[(size_t) juce::jlimit(0, 3, lane)];
}

juce::Rectangle<int> portion(juce::Rectangle<int> r, int index, int count)
{
    const int x1 = r.getX() + r.getWidth() * index / count;
    const int x2 = r.getX() + r.getWidth() * (index + 1) / count;
    return r.withX(x1).withWidth(x2 - x1);
}
}

//──────────────────────────────────────────────────────────────────────────────
void ModStepSequencerPanel::SolidButton::setSolidColour(juce::Colour colour)
{
    solidColour = colour;
    repaint();
}

void ModStepSequencerPanel::SolidButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    auto colour = solidColour;

    if (!isEnabled())
        colour = juce::Colour(0xff1a1e20);
    else if (getToggleState() || down)
        colour = colour.brighter(0.2f);
    else if (highlighted)
        colour = colour.brighter(0.1f);

    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour((getToggleState() ? juce::Colours::white : juce::Colours::black).withAlpha(isEnabled() ? 0.45f : 0.2f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, getToggleState() ? 1.6f : 0.8f);

    const bool darkText = colour.getPerceivedBrightness() > 0.48f && isEnabled();
    g.setColour(isEnabled() ? (darkText ? juce::Colours::black : juce::Colours::white)
                            : IndustrialTheme::mutedText());
    g.setFont(juce::Font(juce::FontOptions { 9.0f, juce::Font::bold }));
    g.drawFittedText(getButtonText(), getLocalBounds().reduced(2), juce::Justification::centred, 1);
}

//──────────────────────────────────────────────────────────────────────────────
ModStepSequencerPanel::StepGrid::StepGrid(juce::AudioProcessorValueTreeState& a,
                                          CurrentStepProvider provider)
    : apvts(a),
      currentStepProvider(std::move(provider))
{
    lengthValue = apvts.getRawParameterValue(StepSequencerMod::kLengthParamID);
    loopLengthValue = apvts.getRawParameterValue(StepSequencerMod::kLoopLengthParamID);

    for (int lane = 0; lane < StepSequencerMod::kNumLanes; ++lane)
    {
        for (int step = 0; step < StepSequencerMod::kNumSteps; ++step)
        {
            const auto index = indexFor(lane, step);
            const auto id = StepSequencerMod::stepParamID(lane, step);
            stepValues[index] = apvts.getRawParameterValue(id);
            stepParams[index] = apvts.getParameter(id);
        }
    }
}

void ModStepSequencerPanel::StepGrid::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff030506));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(IndustrialTheme::panelEdge());
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    const int currentStep = currentStepProvider ? currentStepProvider() : -1;
    const int startStep = segmentStart();
    const int sequenceLength = getSequenceLength();
    const int activeSequenceLength = getActiveSequenceLength();

    for (int lane = 0; lane < StepSequencerMod::kNumLanes; ++lane)
    {
        auto laneRect = laneBounds(lane);
        const auto accent = laneAccent(lane);

        g.setColour(accent.withAlpha(0.08f));
        g.fillRoundedRectangle(laneRect, 3.0f);
        g.setColour(accent.withAlpha(0.4f));
        g.drawRoundedRectangle(laneRect, 3.0f, 1.0f);

        const float stepW = laneRect.getWidth() / (float) StepSequencerMod::kSegmentLength;
        for (int visibleStep = 0; visibleStep < StepSequencerMod::kSegmentLength; ++visibleStep)
        {
            const int step = startStep + visibleStep;
            const bool stepIsInSequence = step < sequenceLength;
            const bool stepIsActive = step < activeSequenceLength;
            auto cell = juce::Rectangle<float>(
                laneRect.getX() + (float) visibleStep * stepW,
                laneRect.getY(),
                stepW,
                laneRect.getHeight()).reduced(1.5f);

            const float value = getStepValue(lane, step);
            auto fill = cell.withTop(cell.getBottom() - cell.getHeight() * value);

            g.setColour(juce::Colour(0xff050708));
            g.fillRoundedRectangle(cell, 2.0f);
            g.setColour(accent.withAlpha(stepIsActive ? 0.75f : (stepIsInSequence ? 0.28f : 0.12f)));
            g.fillRoundedRectangle(fill, 2.0f);
            g.setColour(accent.withAlpha(stepIsActive ? 0.28f : (stepIsInSequence ? 0.18f : 0.08f)));
            g.drawRoundedRectangle(cell, 2.0f, 0.8f);

            if (stepIsActive && step == currentStep)
            {
                g.setColour(juce::Colours::white.withAlpha(0.88f));
                g.drawRoundedRectangle(cell.expanded(0.5f), 2.5f, 1.6f);
            }

            g.setColour((stepIsInSequence ? IndustrialTheme::mutedText() : IndustrialTheme::panelEdge()).withAlpha(stepIsActive ? 0.7f : 0.45f));
            g.setFont(juce::Font(juce::FontOptions { 9.0f, juce::Font::bold }));
            g.drawText(juce::String(step + 1), cell.withHeight(12.0f), juce::Justification::centred);
        }
    }
}

void ModStepSequencerPanel::StepGrid::mouseDown(const juce::MouseEvent& e)
{
    updateFromMouse(e.getPosition());
}

void ModStepSequencerPanel::StepGrid::mouseDrag(const juce::MouseEvent& e)
{
    updateFromMouse(e.getPosition());
}

void ModStepSequencerPanel::StepGrid::randomizeLane(int lane)
{
    const int start = segmentStart();
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        setStepValue(lane, start + step, rng.nextFloat());
}

void ModStepSequencerPanel::StepGrid::mirrorLane(int lane)
{
    const int start = segmentStart();
    std::array<float, StepSequencerMod::kSegmentLength> values {};
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        values[(size_t) step] = getStepValue(lane, start + step);

    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        setStepValue(lane, start + step, values[(size_t) (StepSequencerMod::kSegmentLength - 1 - step)]);
}

void ModStepSequencerPanel::StepGrid::shiftLane(int lane, int direction)
{
    const int start = segmentStart();
    std::array<float, StepSequencerMod::kSegmentLength> values {};
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        values[(size_t) step] = getStepValue(lane, start + step);

    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
    {
        const int src = (step - direction + StepSequencerMod::kSegmentLength) % StepSequencerMod::kSegmentLength;
        setStepValue(lane, start + step, values[(size_t) src]);
    }
}

void ModStepSequencerPanel::StepGrid::riseLane(int lane)
{
    const int start = segmentStart();
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        setStepValue(lane, start + step, (float) (step + 1) / (float) StepSequencerMod::kSegmentLength);
}

void ModStepSequencerPanel::StepGrid::fallLane(int lane)
{
    const int start = segmentStart();
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        setStepValue(lane, start + step, 1.0f - (float) step / (float) (StepSequencerMod::kSegmentLength - 1));
}

void ModStepSequencerPanel::StepGrid::sweepLane(int lane)
{
    const int start = segmentStart();
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
    {
        const float phase = juce::MathConstants<float>::twoPi * (float) step
                          / (float) StepSequencerMod::kSegmentLength;
        setStepValue(lane, start + step, 0.5f + 0.5f * std::sin(phase));
    }
}

void ModStepSequencerPanel::StepGrid::upDownLane(int lane)
{
    const int start = segmentStart();
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        setStepValue(lane, start + step, step % 2 == 0 ? 1.0f : 0.0f);
}

void ModStepSequencerPanel::StepGrid::invertLane(int lane)
{
    const int start = segmentStart();
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        setStepValue(lane, start + step, 1.0f - getStepValue(lane, start + step));
}

void ModStepSequencerPanel::StepGrid::sparseLane(int lane)
{
    const int start = segmentStart();
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        setStepValue(lane, start + step, rng.nextFloat() < 0.32f ? rng.nextFloat() * 0.4f + 0.6f : 0.0f);
}

void ModStepSequencerPanel::StepGrid::algoLane(int lane)
{
    const int start = segmentStart();
    float value = 0.5f;
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
    {
        value = juce::jlimit(0.0f, 1.0f, value + (rng.nextFloat() - 0.5f) * 0.45f);
        setStepValue(lane, start + step, value);
    }
}

void ModStepSequencerPanel::StepGrid::quantizeLane(int lane)
{
    const int start = segmentStart();
    for (int step = 0; step < StepSequencerMod::kSegmentLength; ++step)
        setStepValue(lane, start + step, std::round(getStepValue(lane, start + step) * 4.0f) * 0.25f);
}

void ModStepSequencerPanel::StepGrid::setActiveSegment(int segment)
{
    activeSegment = juce::jlimit(0, StepSequencerMod::kNumSegments - 1, segment);
    repaint();
}

float ModStepSequencerPanel::StepGrid::getStepValue(int lane, int step) const
{
    if (auto* value = stepValues[indexFor(lane, step)])
        return juce::jlimit(0.0f, 1.0f, value->load(std::memory_order_relaxed));

    return 0.0f;
}

void ModStepSequencerPanel::StepGrid::setStepValue(int lane, int step, float value)
{
    auto* param = stepParams[indexFor(lane, step)];
    if (param == nullptr)
        return;

    const float clamped = juce::jlimit(0.0f, 1.0f, value);
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->convertTo0to1(clamped));
    param->endChangeGesture();
    repaint();
}

void ModStepSequencerPanel::StepGrid::updateFromMouse(juce::Point<int> position)
{
    auto laneArea = getLocalBounds().toFloat().reduced(7.0f);
    const float gap = 7.0f;
    const float laneH = (laneArea.getHeight() - gap * (float) (StepSequencerMod::kNumLanes - 1))
                      / (float) StepSequencerMod::kNumLanes;
    const int lane = juce::jlimit(0, StepSequencerMod::kNumLanes - 1,
                                  (int) std::floor(((float) position.y - laneArea.getY())
                                                   / juce::jmax(1.0f, laneH + gap)));
    auto bounds = laneBounds(lane);
    const int step = stepAtX((float) position.x);
    const float value = 1.0f - ((float) position.y - bounds.getY())
                              / juce::jmax(1.0f, bounds.getHeight());
    setStepValue(lane, step, value);
}

juce::Rectangle<float> ModStepSequencerPanel::StepGrid::laneBounds(int lane) const
{
    auto area = getLocalBounds().toFloat().reduced(7.0f);
    const float gap = 7.0f;
    const float laneH = (area.getHeight() - gap * (float) (StepSequencerMod::kNumLanes - 1))
                      / (float) StepSequencerMod::kNumLanes;
    return { area.getX(),
             area.getY() + (float) lane * (laneH + gap),
             area.getWidth(),
             laneH };
}

int ModStepSequencerPanel::StepGrid::stepAtX(float x) const
{
    auto area = getLocalBounds().toFloat().reduced(7.0f);
    const float normalized = (x - area.getX()) / juce::jmax(1.0f, area.getWidth());
    const int visibleStep = juce::jlimit(0, StepSequencerMod::kSegmentLength - 1,
                                         (int) std::floor(normalized * (float) StepSequencerMod::kSegmentLength));
    return juce::jlimit(0, StepSequencerMod::kNumSteps - 1, segmentStart() + visibleStep);
}

int ModStepSequencerPanel::StepGrid::segmentStart() const noexcept
{
    return activeSegment * StepSequencerMod::kSegmentLength;
}

int ModStepSequencerPanel::StepGrid::getSequenceLength() const noexcept
{
    const int index = lengthValue != nullptr ? (int) lengthValue->load(std::memory_order_relaxed) : 1;
    return StepSequencerMod::sequenceLengthForIndex(index);
}

int ModStepSequencerPanel::StepGrid::getActiveSequenceLength() const noexcept
{
    const int loopLength = loopLengthValue != nullptr
        ? (int) loopLengthValue->load(std::memory_order_relaxed)
        : StepSequencerMod::kNumSteps;

    return StepSequencerMod::effectiveSequenceLength(getSequenceLength(), loopLength);
}

size_t ModStepSequencerPanel::StepGrid::indexFor(int lane, int step)
{
    const int safeLane = juce::jlimit(0, StepSequencerMod::kNumLanes - 1, lane);
    const int safeStep = juce::jlimit(0, StepSequencerMod::kNumSteps - 1, step);
    return (size_t) (safeLane * StepSequencerMod::kNumSteps + safeStep);
}

//──────────────────────────────────────────────────────────────────────────────
ModStepSequencerPanel::ModStepSequencerPanel(juce::AudioProcessorValueTreeState& a,
                                             CurrentStepProvider provider)
    : apvts(a),
      currentStepProvider(std::move(provider)),
      grid(apvts, currentStepProvider)
{
    const auto accent = IndustrialTheme::accentMod();

    titleLabel.setText("MOD STEP SEQUENCER", juce::dontSendNotification);
    IndustrialTheme::styleTitle(titleLabel, accent, 13.0f);
    addAndMakeVisible(titleLabel);

    depthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    depthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 16);
    depthSlider.setTooltip("Alien Sequencer");
    IndustrialTheme::styleSlider(depthSlider, accent);
    addAndMakeVisible(depthSlider);
    depthAtt = std::make_unique<Att>(apvts, StepSequencerMod::kDepthParamID, depthSlider);

    sequenceLengthSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    sequenceLengthSlider.setRange(8.0, 32.0, 8.0);
    sequenceLengthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
    sequenceLengthSlider.setNumDecimalPlacesToDisplay(0);
    sequenceLengthSlider.setTooltip("Stored mod sequencer pattern length");
    sequenceLengthSlider.onValueChange = [this]
    {
        const int steps = juce::roundToInt(sequenceLengthSlider.getValue());
        setSequenceLengthIndex(juce::jlimit(0, 3, steps / StepSequencerMod::kSegmentLength - 1));
    };
    IndustrialTheme::styleSlider(sequenceLengthSlider, accent);
    addAndMakeVisible(sequenceLengthSlider);

    loopLengthSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    loopLengthSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 16);
    loopLengthSlider.setNumDecimalPlacesToDisplay(0);
    loopLengthSlider.setTooltip("Performance loop length: active mod sequencer steps");
    IndustrialTheme::styleSlider(loopLengthSlider, accent);
    addAndMakeVisible(loopLengthSlider);
    loopLengthAtt = std::make_unique<Att>(apvts, StepSequencerMod::kLoopLengthParamID, loopLengthSlider);

    divisionBox.addItemList({ "1/4", "1/8", "1/16", "1/32", "1/64", "1/8T", "1/16T", "1/8D", "1/16D" }, 1);
    divisionBox.setTooltip("Mod sequencer base division");
    IndustrialTheme::styleCombo(divisionBox, accent);
    addAndMakeVisible(divisionBox);
    divisionAtt = std::make_unique<CAtt>(apvts, StepSequencerMod::kDivisionParamID, divisionBox);

    subdivisionBox.addItemList({ "x1", "x2", "x4", "x8" }, 1);
    subdivisionBox.setTooltip("Granular Subdivision");
    IndustrialTheme::styleCombo(subdivisionBox, accent);
    addAndMakeVisible(subdivisionBox);
    subdivisionAtt = std::make_unique<CAtt>(apvts, StepSequencerMod::kSubdivisionParamID, subdivisionBox);
    lengthParam = apvts.getParameter(StepSequencerMod::kLengthParamID);
    lengthValue = apvts.getRawParameterValue(StepSequencerMod::kLengthParamID);

    auto makeLabel = [&](juce::Label& label, const juce::String& text, juce::Colour colour)
    {
        label.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(label, colour, 10.0f, juce::Justification::centredLeft);
        addAndMakeVisible(label);
    };

    makeLabel(depthLabel, "DEPTH", accent);
    makeLabel(sequenceLengthLabel, "SEQ LENGTH", accent);
    makeLabel(loopLengthLabel, "LOOP LEN", accent);
    makeLabel(divisionLabel, "DIVISION", accent);
    makeLabel(subdivisionLabel, "SUBDIV", accent);
    makeLabel(lengthLabel, "PATTERN LEN", accent);
    makeLabel(segmentLabel, "SEGMENT", accent);

    const juce::StringArray lengthNames { "8", "16", "24", "32" };
    for (int i = 0; i < 4; ++i)
    {
        auto& button = lengthButtons[(size_t) i];
        button.setButtonText(lengthNames[i]);
        button.setTooltip("Set sequencer length to " + lengthNames[i] + " steps");
        button.setClickingTogglesState(false);
        button.setSolidColour(kPatternColours[(i + 2) % kPatternCount]);
        button.onClick = [this, i] { setSequenceLengthIndex(i); };
        addAndMakeVisible(button);
    }

    const juce::StringArray segmentNames { "1-8", "9-16", "17-24", "25-32" };
    for (int i = 0; i < StepSequencerMod::kNumSegments; ++i)
    {
        auto& button = segmentButtons[(size_t) i];
        button.setButtonText(segmentNames[i]);
        button.setTooltip("Edit steps " + segmentNames[i]);
        button.setClickingTogglesState(false);
        button.setSolidColour(kPatternColours[(i + 6) % kPatternCount]);
        button.onClick = [this, i] { setActiveSegment(i); };
        addAndMakeVisible(button);
    }

    addAndMakeVisible(grid);

    for (int lane = 0; lane < StepSequencerMod::kNumLanes; ++lane)
    {
        const auto laneColour = laneAccent(lane);
        makeLabel(laneLabels[(size_t) lane], "SEQ " + juce::String(lane + 1), laneColour);
        makeLabel(targetLabels[(size_t) lane], "TARGET", laneColour);

        auto& targetBox = targetBoxes[(size_t) lane];
        targetBox.addItemList(ModNames::destDisplayNames(), 1);
        targetBox.setTooltip("SEQ " + juce::String(lane + 1) + " Target");
        IndustrialTheme::styleCombo(targetBox, laneColour);
        addAndMakeVisible(targetBox);
        targetAtts[(size_t) lane] = std::make_unique<CAtt>(
            apvts, StepSequencerMod::laneTargetParamID(lane), targetBox);

        for (int i = 0; i < kPatternCount; ++i)
        {
            auto& button = patternButtons[(size_t) lane][(size_t) i];
            button.setButtonText(kPatternLabels[i]);
            button.setTooltip(kPatternTooltips[i]);
            button.setSolidColour(kPatternColours[i].interpolatedWith(laneColour, 0.18f));
            button.onClick = [this, lane, i] { applyPattern(lane, i); };
            addAndMakeVisible(button);
        }
    }

    updateLengthAndSegmentButtons();
    startTimerHz(30);
}

ModStepSequencerPanel::~ModStepSequencerPanel()
{
    stopTimer();
}

void ModStepSequencerPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintPanel(g, getLocalBounds(), IndustrialTheme::accentMod(), true);
}

void ModStepSequencerPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    titleLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(5);

    auto controlBand = area.removeFromTop(80);
    auto labelRow = controlBand.removeFromTop(14);
    depthLabel.setBounds(labelRow.removeFromLeft(184));
    labelRow.removeFromLeft(8);
    sequenceLengthLabel.setBounds(labelRow.removeFromLeft(84));
    labelRow.removeFromLeft(8);
    loopLengthLabel.setBounds(labelRow.removeFromLeft(84));
    labelRow.removeFromLeft(8);
    divisionLabel.setBounds(labelRow.removeFromLeft(104));
    labelRow.removeFromLeft(8);
    subdivisionLabel.setBounds(labelRow.removeFromLeft(84));

    auto controlRow = controlBand.removeFromTop(60);
    depthSlider.setBounds(controlRow.removeFromLeft(184));
    controlRow.removeFromLeft(8);
    sequenceLengthSlider.setBounds(controlRow.removeFromLeft(84));
    controlRow.removeFromLeft(8);
    loopLengthSlider.setBounds(controlRow.removeFromLeft(84));
    controlRow.removeFromLeft(8);
    divisionBox.setBounds(controlRow.removeFromLeft(104).withHeight(26));
    controlRow.removeFromLeft(8);
    subdivisionBox.setBounds(controlRow.removeFromLeft(84).withHeight(26));
    controlRow.removeFromLeft(12);
    labelRow.removeFromLeft(12);
    lengthLabel.setBounds(labelRow.removeFromLeft(176));
    auto lengthArea = controlRow.removeFromLeft(176);
    for (int i = 0; i < 4; ++i)
        lengthButtons[(size_t) i].setBounds(portion(lengthArea, i, 4).reduced(2, 1));

    area.removeFromTop(8);

    const int rightW = juce::jlimit(310, 390, area.getWidth() / 3);
    auto labelStrip = area.removeFromLeft(54);
    auto laneControls = area.removeFromRight(rightW);
    area.removeFromRight(8);
    auto gridArea = area;
    auto segmentRow = gridArea.removeFromBottom(28);
    gridArea.removeFromBottom(5);
    grid.setBounds(gridArea);

    segmentLabel.setBounds(segmentRow.removeFromLeft(62));
    for (int i = 0; i < StepSequencerMod::kNumSegments; ++i)
        segmentButtons[(size_t) i].setBounds(portion(segmentRow, i, StepSequencerMod::kNumSegments).reduced(3, 2));

    const int laneGap = 7;
    const int laneH = (grid.getHeight() - laneGap * (StepSequencerMod::kNumLanes - 1))
                    / StepSequencerMod::kNumLanes;

    for (int lane = 0; lane < StepSequencerMod::kNumLanes; ++lane)
    {
        const int y = grid.getY() + 7 + lane * (laneH + laneGap);
        laneLabels[(size_t) lane].setBounds(labelStrip.withY(y).withHeight(laneH));

        auto laneRow = laneControls.withY(y).withHeight(laneH);
        auto targetRow = laneRow.removeFromTop(24);
        targetLabels[(size_t) lane].setBounds(targetRow.removeFromLeft(58));
        targetBoxes[(size_t) lane].setBounds(targetRow.reduced(2, 0));
        laneRow.removeFromTop(4);

        auto buttonRowA = laneRow.removeFromTop(22);
        auto buttonRowB = laneRow.removeFromTop(22);
        auto buttonRowC = laneRow.removeFromTop(22);
        for (int i = 0; i < 4; ++i)
        {
            patternButtons[(size_t) lane][(size_t) i].setBounds(portion(buttonRowA, i, 4).reduced(2, 1));
            patternButtons[(size_t) lane][(size_t) (i + 4)].setBounds(portion(buttonRowB, i, 4).reduced(2, 1));
            patternButtons[(size_t) lane][(size_t) (i + 8)].setBounds(portion(buttonRowC, i, 4).reduced(2, 1));
        }
    }
}

void ModStepSequencerPanel::timerCallback()
{
    updateLengthAndSegmentButtons();
    grid.repaint();
}

void ModStepSequencerPanel::applyPattern(int lane, int patternIndex)
{
    switch (patternIndex)
    {
        case 0: grid.randomizeLane(lane); break;
        case 1: grid.mirrorLane(lane); break;
        case 2: grid.shiftLane(lane, -1); break;
        case 3: grid.shiftLane(lane, 1); break;
        case 4: grid.riseLane(lane); break;
        case 5: grid.fallLane(lane); break;
        case 6: grid.sweepLane(lane); break;
        case 7: grid.upDownLane(lane); break;
        case 8: grid.invertLane(lane); break;
        case 9: grid.sparseLane(lane); break;
        case 10: grid.algoLane(lane); break;
        case 11: grid.quantizeLane(lane); break;
        default: break;
    }
}

void ModStepSequencerPanel::setSequenceLengthIndex(int index)
{
    if (lengthParam == nullptr)
        return;

    const int safeIndex = juce::jlimit(0, 3, index);
    lengthParam->beginChangeGesture();
    lengthParam->setValueNotifyingHost(lengthParam->convertTo0to1((float) safeIndex));
    lengthParam->endChangeGesture();

    const int maxSegment = StepSequencerMod::sequenceLengthForIndex(safeIndex) / StepSequencerMod::kSegmentLength - 1;
    if (grid.getActiveSegment() > maxSegment)
        setActiveSegment(maxSegment);

    updateLengthAndSegmentButtons();
}

int ModStepSequencerPanel::getSequenceLengthIndex() const noexcept
{
    return lengthValue != nullptr
        ? juce::jlimit(0, 3, (int) lengthValue->load(std::memory_order_relaxed))
        : 1;
}

int ModStepSequencerPanel::getSequenceLength() const noexcept
{
    return StepSequencerMod::sequenceLengthForIndex(getSequenceLengthIndex());
}

void ModStepSequencerPanel::setActiveSegment(int segment)
{
    const int maxSegment = getSequenceLength() / StepSequencerMod::kSegmentLength - 1;
    grid.setActiveSegment(juce::jlimit(0, maxSegment, segment));
    updateLengthAndSegmentButtons();
}

void ModStepSequencerPanel::updateLengthAndSegmentButtons()
{
    const int lengthIndex = getSequenceLengthIndex();
    const int activeSegment = grid.getActiveSegment();
    const int enabledSegments = getSequenceLength() / StepSequencerMod::kSegmentLength;

    for (int i = 0; i < 4; ++i)
        lengthButtons[(size_t) i].setToggleState(i == lengthIndex, juce::dontSendNotification);

    sequenceLengthSlider.setValue((double) getSequenceLength(), juce::dontSendNotification);

    if (activeSegment >= enabledSegments)
        grid.setActiveSegment(enabledSegments - 1);

    for (int i = 0; i < StepSequencerMod::kNumSegments; ++i)
    {
        auto& button = segmentButtons[(size_t) i];
        button.setEnabled(i < enabledSegments);
        button.setToggleState(i == grid.getActiveSegment(), juce::dontSendNotification);
    }
}
