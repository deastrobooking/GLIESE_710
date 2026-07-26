#include "MainPanel.h"
#include "../Engine/Oscillators/FMAlgorithm.h"
#include <vector>

static juce::Colour kBg { 0xff050607 };

namespace
{
class FMAlgorithmGraph : public juce::Component,
                         public juce::SettableTooltipClient,
                         private juce::Timer
{
public:
    FMAlgorithmGraph(juce::AudioProcessorValueTreeState& state,
                     const juce::String& parameterId)
        : algorithmParam(state.getRawParameterValue(parameterId))
    {
        setTooltip("Live FM routing: arrows run from modulator to destination; filled nodes are carriers");
        setInterceptsMouseClicks(false, false);
        startTimerHz(15);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090a));
        g.fillRoundedRectangle(bounds, 3.f);
        g.setColour(IndustrialTheme::accentFM().withAlpha(0.25f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 3.f, 0.8f);

        const auto& algorithm = FMAlgorithm::getAlgorithmDef(currentAlgorithm);
        std::array<juce::Point<float>, FMAlgorithm::kNumOperators> nodes;
        const auto graph = bounds.reduced(10.f, 7.f);
        const auto centre = graph.getCentre();
        const float radiusX = graph.getWidth() * 0.42f;
        const float radiusY = graph.getHeight() * 0.39f;

        for (int op = 0; op < FMAlgorithm::kNumOperators; ++op)
        {
            const float angle = -juce::MathConstants<float>::halfPi
                              + juce::MathConstants<float>::twoPi * (float)op
                                / (float)FMAlgorithm::kNumOperators;
            nodes[(size_t)op] = { centre.x + std::cos(angle) * radiusX,
                                  centre.y + std::sin(angle) * radiusY };
        }

        const auto accent = IndustrialTheme::accentFM();
        for (int destination = 0; destination < FMAlgorithm::kNumOperators; ++destination)
        {
            const uint16_t mask = algorithm.opModulators[destination];
            for (int modulator = 0; modulator < FMAlgorithm::kNumOperators; ++modulator)
            {
                if ((mask & (uint16_t)(1u << modulator)) == 0)
                    continue;

                const auto from = nodes[(size_t)modulator];
                const auto to = nodes[(size_t)destination];
                g.setColour(accent.withAlpha(0.34f));

                if (modulator == destination)
                {
                    g.drawEllipse(from.x - 7.f, from.y - 9.f, 14.f, 11.f, 0.8f);
                    continue;
                }

                auto delta = to - from;
                const float length = juce::jmax(0.001f, delta.getDistanceFromOrigin());
                const auto unit = delta / length;
                const auto edgeStart = from + unit * 5.f;
                const auto edgeEnd = to - unit * 5.f;
                g.drawLine({ edgeStart, edgeEnd }, 0.8f);

                const auto normal = juce::Point<float> { -unit.y, unit.x };
                juce::Path arrow;
                arrow.startNewSubPath(edgeEnd);
                arrow.lineTo(edgeEnd - unit * 4.f + normal * 2.f);
                arrow.lineTo(edgeEnd - unit * 4.f - normal * 2.f);
                arrow.closeSubPath();
                g.fillPath(arrow);
            }
        }

        for (int op = 0; op < FMAlgorithm::kNumOperators; ++op)
        {
            const bool carrier = (algorithm.carriers & (uint16_t)(1u << op)) != 0;
            const auto point = nodes[(size_t)op];
            g.setColour(carrier ? accent.withAlpha(0.88f) : juce::Colour(0xff171b1d));
            g.fillEllipse(point.x - 5.f, point.y - 5.f, 10.f, 10.f);
            g.setColour(carrier ? juce::Colours::white.withAlpha(0.75f)
                                : accent.withAlpha(0.72f));
            g.drawEllipse(point.x - 5.f, point.y - 5.f, 10.f, 10.f, 0.8f);
            g.setFont(juce::Font(juce::FontOptions { 6.5f }));
            g.drawText(juce::String(op + 1),
                       juce::Rectangle<float>(point.x - 5.f, point.y - 5.f, 10.f, 10.f),
                       juce::Justification::centred);
        }
    }

private:
    void timerCallback() override
    {
        const int next = algorithmParam != nullptr
            ? juce::jlimit(0, FMAlgorithm::kNumAlgorithms - 1,
                           juce::roundToInt(algorithmParam->load()))
            : 0;
        if (next != currentAlgorithm)
        {
            currentAlgorithm = next;
            repaint();
        }
    }

    std::atomic<float>* algorithmParam = nullptr;
    int currentAlgorithm = 0;
};

class FMEngineEditor : public juce::Component
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;
    using Att   = APVTS::SliderAttachment;
    using CAtt  = APVTS::ComboBoxAttachment;
    using BAtt  = APVTS::ButtonAttachment;

    FMEngineEditor(APVTS& state, int index)
        : apvts(state),
          engineIndex(index),
          fmPrefix(index == 0 ? "osc_fm_" : "osc_fm2_"),
          opBase(index == 0 ? "op" : "op2_"),
          algorithmGraph(state, index == 0 ? "osc_fm_algorithm" : "osc_fm2_algorithm")
    {
        const auto accent = IndustrialTheme::accentFM();
        title.setText("FM ENGINE " + juce::String(engineIndex == 0 ? "A" : "B"), juce::dontSendNotification);
        IndustrialTheme::styleTitle(title, accent, 13.f);
        addAndMakeVisible(title);

        enableButton.setButtonText("ON");
        enableButton.setTooltip("Enable / disable this FM engine");
        enableButton.setColour(juce::ToggleButton::textColourId, accent);
        enableButton.setColour(juce::ToggleButton::tickColourId, accent);
        enableButton.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff555555));
        addAndMakeVisible(enableButton);
        enableAtt = std::make_unique<BAtt>(apvts, fmPrefix + "on", enableButton);

        algorithmBox.addItemList([]
        {
            juce::StringArray items;
            for (int i = 0; i < 40; ++i)
                items.add("Algorithm " + juce::String(i));
            return items;
        }(), 1);
        algorithmBox.setTooltip("FM Algorithm");
        IndustrialTheme::styleCombo(algorithmBox, accent);
        addAndMakeVisible(algorithmBox);
        addAndMakeVisible(algorithmGraph);
        algorithmAtt = std::make_unique<CAtt>(apvts, fmPrefix + "algorithm", algorithmBox);

        setupRotary(levelSlider,  fmPrefix + "level",      "FM Level", levelAtt, 52);
        setupRotary(tuneSlider,   fmPrefix + "tune",       "FM Fine Tune (cents)", tuneAtt, 52);
        setupRotary(octaveSlider, fmPrefix + "octave",     "FM Octave", octaveAtt, 52);
        setupRotary(detuneSlider, fmPrefix + "detune",     "Master Detune", detuneAtt, 60);
        setupRotary(phaseSlider,  fmPrefix + "phase",      "Phase Position", phaseAtt, 60);
        setupRotary(morphSlider,  fmPrefix + "algo_morph", "Algorithm Morph", morphAtt, 60);
        setupRotary(indexSlider,  fmPrefix + "index",      "Modulation Index Scale", indexAtt, 60);

        setupRotary(envA, fmPrefix + "adsr_a", "FM Attack",  envAAtt, 42);
        setupRotary(envD, fmPrefix + "adsr_d", "FM Decay",   envDAtt, 42);
        setupRotary(envS, fmPrefix + "adsr_s", "FM Sustain", envSAtt, 42);
        setupRotary(envR, fmPrefix + "adsr_r", "FM Release", envRAtt, 42);
        for (auto* slider : { &envA, &envD, &envS, &envR })
        {
            slider->setSliderStyle(juce::Slider::LinearVertical);
            slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 42, 13);
        }

        static const juce::StringArray kOpWaves {
            "Sine","Triangle","Saw","Square","Pulse","Noise",
            "All Wide","All Narrow","Odd Wide","Odd Narrow","Res Wide","Res Narrow"
        };

        for (int i = 0; i < 10; ++i)
        {
            const juce::String pfx = opBase + juce::String(i) + "_";
            setupOpRotary(opCoarse[(size_t)i], pfx + "coarse", "Op " + juce::String(i) + " Coarse", coarseAtts[(size_t)i]);
            setupOpRotary(opFine[(size_t)i],   pfx + "fine",   "Op " + juce::String(i) + " Fine", fineAtts[(size_t)i]);
            setupOpRotary(opLevel[(size_t)i],  pfx + "level",  "Op " + juce::String(i) + " Level", levelAtts[(size_t)i]);
            setupOpFader(opDetune[(size_t)i],   pfx + "detune",   "Op " + juce::String(i) + " Detune", detuneAtts[(size_t)i]);
            setupOpFader(opFeedback[(size_t)i], pfx + "feedback", "Op " + juce::String(i) + " Feedback", feedbackAtts[(size_t)i]);
            setupOpFader(opSkirt[(size_t)i],    pfx + "skirt",    "Op " + juce::String(i) + " Spectral Skirt", skirtAtts[(size_t)i]);
            setupOpFader(opRes[(size_t)i],      pfx + "resonance","Op " + juce::String(i) + " Spectral Resonance", resAtts[(size_t)i]);
            setupOpFader(opVel[(size_t)i],      pfx + "vel",      "Op " + juce::String(i) + " Velocity Depth", velAtts[(size_t)i]);
            setupOpFader(opKey[(size_t)i],      pfx + "key",      "Op " + juce::String(i) + " Key Scale", keyAtts[(size_t)i]);

            opWave[(size_t)i].addItemList(kOpWaves, 1);
            opWave[(size_t)i].setTooltip("Op " + juce::String(i) + " Waveform");
            IndustrialTheme::styleCombo(opWave[(size_t)i], IndustrialTheme::accentFM());
            addAndMakeVisible(opWave[(size_t)i]);
            waveAtts[(size_t)i] = std::make_unique<CAtt>(apvts, pfx + "wave", opWave[(size_t)i]);
        }

        auto addLabel = [&](juce::Label& label, const juce::String& text, float size = 8.5f)
        {
            label.setText(text, juce::dontSendNotification);
            IndustrialTheme::styleLabel(label, IndustrialTheme::mutedText(), size);
            label.setFont(juce::Font(juce::FontOptions { size }));
            addAndMakeVisible(label);
        };

        addLabel(algoLabel, "ALGORITHM");
        addLabel(levelLabel, "LEVEL");
        addLabel(tuneLabel, "TUNE");
        addLabel(octaveLabel, "OCTAVE");
        const juce::String envNames[] = { "A", "D", "S", "R" };
        for (int i = 0; i < 4; ++i)
            addLabel(envLabels[(size_t)i], envNames[i]);
        const juce::String perfNames[] = { "DETUNE", "PHASE", "MORPH", "INDEX" };
        for (int i = 0; i < 4; ++i)
            addLabel(perfLabels[(size_t)i], perfNames[i]);

        addLabel(coarseHdr, "C");
        addLabel(fineHdr, "F");
        addLabel(levelHdr, "L");
        addLabel(detuneHdr, "DT");
        addLabel(feedbackHdr, "FB");
        addLabel(skirtHdr, "SK");
        addLabel(resHdr, "RS");
        addLabel(velHdr, "VL");
        addLabel(keyHdr, "KY");
        addLabel(waveHdr, "WAVE");
        for (int i = 0; i < 10; ++i)
            addLabel(opLabels[(size_t)i], "OP " + juce::String(i + 1), 9.f);
    }

    void paint(juce::Graphics& g) override
    {
        IndustrialTheme::paintPanel(g, getLocalBounds(), IndustrialTheme::accentFM(), true);

        if (!operatorGridBounds.isEmpty())
        {
            const auto accent = IndustrialTheme::accentFM();
            for (int i = 0; i < 10; ++i)
            {
                auto strip = operatorGridBounds.withX(operatorGridBounds.getX() + i * operatorColumnWidth)
                                               .withWidth(operatorColumnWidth);
                if ((i / 5) % 2 == 1)
                {
                    g.setColour(juce::Colours::white.withAlpha(0.018f));
                    g.fillRect(strip);
                }
                g.setColour(juce::Colours::white.withAlpha(0.035f));
                g.drawVerticalLine(strip.getX(), (float)strip.getY(), (float)strip.getBottom());

                auto inner = strip.reduced(3, 0);
                const int headerH = 14;
                const int legendH = 10;
                auto knobLegend = inner.withY(inner.getY() + headerH).withHeight(legendH);
                auto faderLegend = inner.withY(inner.getBottom() - legendH).withHeight(legendH);

                g.setColour(IndustrialTheme::mutedText().withAlpha(0.82f));
                g.setFont(juce::Font(juce::FontOptions { 8.f }));
                const juce::String knobNames[] { "C", "F", "L" };
                const juce::String faderNames[] { "DT", "FB", "SK", "RS", "VL", "KY" };
                for (int control = 0; control < 3; ++control)
                    g.drawText(knobNames[control],
                               knobLegend.withX(knobLegend.getX() + control * knobLegend.getWidth() / 3)
                                         .withWidth(knobLegend.getWidth() / 3),
                               juce::Justification::centred);
                for (int control = 0; control < 6; ++control)
                    g.drawText(faderNames[control],
                               faderLegend.withX(faderLegend.getX() + control * faderLegend.getWidth() / 6)
                                          .withWidth(faderLegend.getWidth() / 6),
                               juce::Justification::centred);
            }

            const int dividerX = operatorGridBounds.getX() + 5 * operatorColumnWidth;
            g.setColour(accent.withAlpha(0.22f));
            g.drawVerticalLine(dividerX, (float)operatorGridBounds.getY(),
                               (float)operatorGridBounds.getBottom());
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto titleRow = area.removeFromTop(22);
        enableButton.setBounds(titleRow.removeFromRight(48));
        title.setBounds(titleRow.removeFromLeft(190));
        area.removeFromTop(4);

        auto topBand = area.removeFromTop(116);
        const int perfW = juce::jlimit(248, 344, topBand.getWidth() / 3);
        auto perfBand = topBand.removeFromRight(perfW);
        topBand.removeFromRight(8);

        auto perfLabelRow = perfBand.removeFromTop(13);
        const int perfKnobW = juce::jmax(58, perfBand.getWidth() / 4);
        for (auto& label : perfLabels)
            label.setBounds(perfLabelRow.removeFromLeft(perfKnobW));
        auto perfRow = perfBand;
        detuneSlider.setBounds(perfRow.removeFromLeft(perfKnobW).reduced(2));
        phaseSlider .setBounds(perfRow.removeFromLeft(perfKnobW).reduced(2));
        morphSlider .setBounds(perfRow.removeFromLeft(perfKnobW).reduced(2));
        indexSlider .setBounds(perfRow.removeFromLeft(perfKnobW).reduced(2));

        auto topLabelRow = topBand.removeFromTop(13);
        algoLabel .setBounds(topLabelRow.removeFromLeft(172));
        levelLabel.setBounds(topLabelRow.removeFromLeft(64));
        tuneLabel .setBounds(topLabelRow.removeFromLeft(64));
        octaveLabel.setBounds(topLabelRow.removeFromLeft(64));
        topLabelRow.removeFromLeft(8);
        for (auto& label : envLabels)
            label.setBounds(topLabelRow.removeFromLeft(44));

        auto topRow = topBand;
        auto algorithmArea = topRow.removeFromLeft(172);
        algorithmBox.setBounds(algorithmArea.removeFromTop(24));
        algorithmArea.removeFromTop(3);
        algorithmGraph.setBounds(algorithmArea);
        levelSlider .setBounds(topRow.removeFromLeft(64));
        tuneSlider  .setBounds(topRow.removeFromLeft(64));
        octaveSlider.setBounds(topRow.removeFromLeft(64));
        topRow.removeFromLeft(8);
        envA.setBounds(topRow.removeFromLeft(44).reduced(2, 0));
        envD.setBounds(topRow.removeFromLeft(44).reduced(2, 0));
        envS.setBounds(topRow.removeFromLeft(44).reduced(2, 0));
        envR.setBounds(topRow.removeFromLeft(44).reduced(2, 0));

        area.removeFromTop(4);
        operatorColumnWidth = juce::jmax(1, area.getWidth() / 10);
        operatorGridBounds = area.withWidth(operatorColumnWidth * 10);
        for (auto* label : { &coarseHdr, &fineHdr, &levelHdr, &detuneHdr, &feedbackHdr,
                             &skirtHdr, &resHdr, &velHdr, &keyHdr, &waveHdr })
            label->setVisible(false);

        const int headerH = 14;
        const int legendH = 10;
        const int knobH = juce::jlimit(22, 34, operatorColumnWidth - 8);
        const int waveH = 20;
        const int faderTop = operatorGridBounds.getY() + headerH + legendH + knobH + waveH + 8;
        const int faderH = juce::jmax(34, operatorGridBounds.getBottom() - faderTop - legendH);

        for (int i = 0; i < 10; ++i)
        {
            auto strip = operatorGridBounds.withX(operatorGridBounds.getX() + i * operatorColumnWidth)
                                           .withWidth(operatorColumnWidth)
                                           .reduced(3, 0);
            opLabels[(size_t)i].setBounds(strip.removeFromTop(headerH));

            strip.removeFromTop(legendH);
            const int knobW = juce::jmax(1, strip.getWidth() / 3);

            auto knobs = strip.removeFromTop(knobH);
            opCoarse[(size_t)i].setBounds(knobs.removeFromLeft(knobW));
            opFine[(size_t)i].setBounds(knobs.removeFromLeft(knobW));
            opLevel[(size_t)i].setBounds(knobs);
            strip.removeFromTop(2);

            opWave[(size_t)i].setBounds(strip.removeFromTop(waveH));
            strip.removeFromTop(4);

            auto faders = strip.removeFromTop(faderH);
            const int faderW = juce::jmax(1, faders.getWidth() / 6);
            opDetune[(size_t)i].setBounds(faders.removeFromLeft(faderW));
            opFeedback[(size_t)i].setBounds(faders.removeFromLeft(faderW));
            opSkirt[(size_t)i].setBounds(faders.removeFromLeft(faderW));
            opRes[(size_t)i].setBounds(faders.removeFromLeft(faderW));
            opVel[(size_t)i].setBounds(faders.removeFromLeft(faderW));
            opKey[(size_t)i].setBounds(faders);
        }

    }

private:
    void setupRotary(juce::Slider& slider, const juce::String& paramId,
                     const juce::String& tooltip, std::unique_ptr<Att>& attachment,
                     int textWidth)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textWidth, 16);
        slider.setTooltip(tooltip);
        IndustrialTheme::styleSlider(slider, IndustrialTheme::accentFM());
        addAndMakeVisible(slider);
        attachment = std::make_unique<Att>(apvts, paramId, slider);
    }

    void setupOpRotary(juce::Slider& slider, const juce::String& paramId,
                       const juce::String& tooltip, std::unique_ptr<Att>& attachment)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled(true, true, this);
        slider.setTooltip(tooltip);
        IndustrialTheme::styleSlider(slider, IndustrialTheme::accentFM());
        slider.getProperties().set("microRotary", true);
        addAndMakeVisible(slider);
        attachment = std::make_unique<Att>(apvts, paramId, slider);
    }

    void setupOpFader(juce::Slider& slider, const juce::String& paramId,
                      const juce::String& tooltip, std::unique_ptr<Att>& attachment)
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled(true, true, this);
        slider.setTooltip(tooltip);
        IndustrialTheme::styleMiniFader(slider, IndustrialTheme::accentFM());
        addAndMakeVisible(slider);
        attachment = std::make_unique<Att>(apvts, paramId, slider);
    }

    APVTS& apvts;
    int engineIndex = 0;
    juce::String fmPrefix, opBase;
    FMAlgorithmGraph algorithmGraph;

    juce::Label title;
    juce::ToggleButton enableButton;
    juce::ComboBox algorithmBox;
    juce::Slider levelSlider, tuneSlider, octaveSlider, detuneSlider, phaseSlider, morphSlider, indexSlider;
    juce::Slider envA, envD, envS, envR;
    juce::Label algoLabel, levelLabel, tuneLabel, octaveLabel;
    std::array<juce::Label, 4> envLabels, perfLabels;

    std::array<juce::Slider, 10> opCoarse, opFine, opDetune, opLevel, opFeedback;
    std::array<juce::ComboBox, 10> opWave;
    std::array<juce::Slider, 10> opSkirt, opRes, opVel, opKey;
    juce::Label coarseHdr, fineHdr, detuneHdr, levelHdr, feedbackHdr, waveHdr;
    juce::Label skirtHdr, resHdr, velHdr, keyHdr;
    std::array<juce::Label, 10> opLabels;

    std::unique_ptr<BAtt> enableAtt;
    std::unique_ptr<CAtt> algorithmAtt;
    std::unique_ptr<Att> levelAtt, tuneAtt, octaveAtt, detuneAtt, phaseAtt, morphAtt, indexAtt;
    std::unique_ptr<Att> envAAtt, envDAtt, envSAtt, envRAtt;
    std::array<std::unique_ptr<Att>, 10> coarseAtts, fineAtts, detuneAtts, levelAtts, feedbackAtts;
    std::array<std::unique_ptr<CAtt>, 10> waveAtts;
    std::array<std::unique_ptr<Att>, 10> skirtAtts, resAtts, velAtts, keyAtts;
    juce::Rectangle<int> operatorGridBounds;
    int operatorColumnWidth = 1;
};

class EngineFilterPage : public juce::Component
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;
    using Att   = APVTS::SliderAttachment;
    using CAtt  = APVTS::ComboBoxAttachment;
    using BAtt  = APVTS::ButtonAttachment;

    explicit EngineFilterPage(APVTS& state) : apvts(state)
    {
        auto accent = IndustrialTheme::accentFilter();
        title.setText("ENGINE FILTERS", juce::dontSendNotification);
        IndustrialTheme::styleTitle(title, accent, 13.f);
        addAndMakeVisible(title);

        static const juce::StringArray filterModes {
            "LP12","LP24","HP12","HP24","Bandpass","Notch",
            "Peak","LoShelf","HiShelf","Comb","AM+","AM-",
            "Ladder12","Ladder18","Ladder24","SEM Morph","MS Dual","Formant",
            "Juno-106","Juno-60","Jupiter-4","Jupiter-8","TB-303",
            "P5 Rev2 SSM","P5 Rev3 CEM","Prophet-600","Prophet-6","Pro-One",
            "GLIESE Prism","Alt Ladder","Phys Ladder"
        };
        juce::StringArray engineFilterModes = filterModes;
        engineFilterModes.add("None");
        static const juce::StringArray oversampling { "Off", "2x", "4x" };
        static const juce::String names[] { "3D", "FM", "WT", "VA", "KS", "GRAN", "TERRAIN" };
        static const juce::String ids[] { "3d", "fm", "wt", "va", "ks", "gran", "terrain" };

        for (int i = 0; i < kRows; ++i)
        {
            auto& row = rows[(size_t) i];
            row.name.setText(names[(size_t) i], juce::dontSendNotification);
            IndustrialTheme::styleLabel(row.name, accent, 10.f, juce::Justification::centredLeft);
            addAndMakeVisible(row.name);

            const juce::String pfx = "engine_filter_" + ids[(size_t) i] + "_";
            row.on.setButtonText("ON");
            IndustrialTheme::styleButton(row.on, accent);
            addAndMakeVisible(row.on);
            row.onAtt = std::make_unique<BAtt>(apvts, pfx + "on", row.on);

            row.type.addItemList(engineFilterModes, 1);
            IndustrialTheme::styleCombo(row.type, accent);
            addAndMakeVisible(row.type);
            row.typeAtt = std::make_unique<CAtt>(apvts, pfx + "type", row.type);

            row.os.addItemList(oversampling, 1);
            IndustrialTheme::styleCombo(row.os, accent);
            addAndMakeVisible(row.os);
            row.osAtt = std::make_unique<CAtt>(apvts, pfx + "oversample", row.os);

            setupSlider(row.cutoff, pfx + "cutoff", "Cutoff", row.cutoffAtt, 50);
            setupSlider(row.res,    pfx + "resonance", "Resonance", row.resAtt, 42);
            setupSlider(row.drive,  pfx + "drive", "Drive", row.driveAtt, 42);
            setupSlider(row.morph,  pfx + "morph", "Morph", row.morphAtt, 42);
            setupSlider(row.sat,    pfx + "saturation", "Saturation", row.satAtt, 42);
            setupSlider(row.bass,   pfx + "bass_comp", "Bass Comp", row.bassAtt, 42);
            setupSlider(row.mix,    pfx + "mix", "Mix", row.mixAtt, 42);
            setupSlider(row.env,    pfx + "env_amt", "Envelope Amount", row.envAtt, 42);
            setupSlider(row.lfo,    pfx + "lfo_amt", "LFO Amount", row.lfoAtt, 42);
        }

        static const juce::String headers[] {
            "ENGINE", "ON", "TYPE", "CUT", "RES", "DRV", "MOR", "SAT", "BASS", "MIX", "ENV", "LFO", "OS"
        };
        for (int i = 0; i < kCols; ++i)
        {
            header[(size_t) i].setText(headers[(size_t) i], juce::dontSendNotification);
            IndustrialTheme::styleLabel(header[(size_t) i], accent, 10.f, juce::Justification::centredLeft);
            addAndMakeVisible(header[(size_t) i]);
        }
    }

    void paint(juce::Graphics& g) override
    {
        IndustrialTheme::paintPanel(g, getLocalBounds(), IndustrialTheme::accentFilter(), true);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        title.setBounds(area.removeFromTop(22));
        area.removeFromTop(4);

        auto headerRow = area.removeFromTop(15);
        layoutRow(headerRow, nullptr, true);
        area.removeFromTop(4);

        const int rowH = juce::jmax(42, area.getHeight() / kRows);
        for (int i = 0; i < kRows; ++i)
        {
            auto rowArea = area.removeFromTop(rowH).reduced(0, 2);
            layoutRow(rowArea, &rows[(size_t) i], false);
        }
    }

private:
    static constexpr int kRows = 7;
    static constexpr int kCols = 13;

    struct Row
    {
        juce::Label name;
        juce::ToggleButton on;
        juce::ComboBox type, os;
        juce::Slider cutoff, res, drive, morph, sat, bass, mix, env, lfo;
        std::unique_ptr<BAtt> onAtt;
        std::unique_ptr<CAtt> typeAtt, osAtt;
        std::unique_ptr<Att> cutoffAtt, resAtt, driveAtt, morphAtt, satAtt, bassAtt, mixAtt, envAtt, lfoAtt;
    };

    void setupSlider(juce::Slider& slider, const juce::String& paramId,
                     const juce::String& tooltip, std::unique_ptr<Att>& attachment, int textWidth)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, textWidth, 13);
        slider.setTooltip(tooltip);
        IndustrialTheme::styleSlider(slider, IndustrialTheme::accentFilter());
        addAndMakeVisible(slider);
        attachment = std::make_unique<Att>(apvts, paramId, slider);
    }

    void layoutRow(juce::Rectangle<int> rowArea, Row* row, bool isHeader)
    {
        const int nameW = 72;
        const int onW = 42;
        const int typeW = juce::jlimit(118, 180, rowArea.getWidth() / 5);
        const int osW = 58;
        const int remaining = rowArea.getWidth() - nameW - onW - typeW - osW;
        const int knobW = juce::jmax(42, remaining / 9);

        auto setHeader = [&](int idx, juce::Rectangle<int> r)
        {
            header[(size_t) idx].setBounds(r);
        };

        if (isHeader)
        {
            setHeader(0, rowArea.removeFromLeft(nameW));
            setHeader(1, rowArea.removeFromLeft(onW));
            setHeader(2, rowArea.removeFromLeft(typeW));
            for (int i = 3; i <= 11; ++i)
                setHeader(i, rowArea.removeFromLeft(knobW));
            setHeader(12, rowArea.removeFromLeft(osW));
            return;
        }

        row->name.setBounds(rowArea.removeFromLeft(nameW));
        row->on.setBounds(rowArea.removeFromLeft(onW).reduced(2, 8));
        auto typeBounds = rowArea.removeFromLeft(typeW);
        row->type.setBounds(typeBounds.withHeight(24).withCentre(typeBounds.getCentre()));

        juce::Slider* knobs[] {
            &row->cutoff, &row->res, &row->drive, &row->morph, &row->sat,
            &row->bass, &row->mix, &row->env, &row->lfo
        };
        for (auto* knob : knobs)
            knob->setBounds(rowArea.removeFromLeft(knobW).reduced(1));

        auto osBounds = rowArea.removeFromLeft(osW);
        row->os.setBounds(osBounds.withHeight(24).withCentre(osBounds.getCentre()));
    }

    APVTS& apvts;
    juce::Label title;
    std::array<juce::Label, kCols> header;
    std::array<Row, kRows> rows;
};
} // namespace

//──────────────────────────────────────────────────────────────────────────────
MainPanel::MainPanel(juce::AudioProcessorValueTreeState& a,
                     TerrainImportCallback terrainImportCallback,
                     GranularImportCallback granularImportCallback,
                     GranularCaptureCallback granularCaptureCallback,
                     ModStepSequencerPanel::CurrentStepProvider stepProvider)
    : apvts(a),
      terrainImport(std::move(terrainImportCallback)),
      granularImport(std::move(granularImportCallback)),
      granularCapture(std::move(granularCaptureCallback)),
      modStepProvider(std::move(stepProvider))
{
    tabs.setTabBarDepth(28);
    tabs.setOutline(0);
    tabs.setColour(juce::TabbedComponent::backgroundColourId,  kBg);
    tabs.setColour(juce::TabbedComponent::outlineColourId,     IndustrialTheme::panelEdge());
    tabs.setColour(juce::TabbedButtonBar::tabTextColourId,     IndustrialTheme::mutedText());
    tabs.setColour(juce::TabbedButtonBar::frontTextColourId,   IndustrialTheme::text());
    tabs.setColour(juce::TabbedButtonBar::tabOutlineColourId,  IndustrialTheme::panelEdge());
    tabs.setColour(juce::TabbedButtonBar::frontOutlineColourId,
                   IndustrialTheme::accentMaster().withAlpha(0.7f));
    addAndMakeVisible(tabs);

    buildOscillatorsTab();
    buildEngineTab();
    buildEngineFiltersTab();
    buildEffectsTab();
    buildSequencerTab();

    // ── Master controls ───────────────────────────────────────────────────────
    masterLabel.setText("MASTER", juce::dontSendNotification);
    IndustrialTheme::styleTitle(masterLabel, IndustrialTheme::accentMaster(), 13.f);
    addAndMakeVisible(masterLabel);

    setupKnob(masterVolSlider,    "master_volume",    "Master Volume",    masterVolAtt,    *this);
    setupKnob(masterTuneSlider,   "master_tune",      "Master Tune",      masterTuneAtt,   *this);
    setupKnob(masterOctaveSlider, "master_octave",    "Master Octave",    masterOctaveAtt, *this);
    setupKnob(masterPanSlider,    "master_pan",       "Master Pan",       masterPanAtt,    *this);
    setupKnob(masterPortaSlider,  "master_portamento","Portamento Time",  masterPortaAtt,  *this);
    setupKnob(masterAgeSlider,    "master_age",       "Age / Bitcrusher", masterAgeAtt,    *this);
    setupKnob(masterFeedSlider,   "master_feedback",  "Output Feedback",  masterFeedAtt,   *this);
    setupKnob(masterPBSlider,     "master_pb_range",  "Pitch Bend Range", masterPBAtt,     *this);
    setupKnob(masterMWSlider,     "master_mw_range",  "Mod Wheel Range",  masterMWAtt,     *this);
    setupKnob(masterA,            "master_adsr_a",    "Attack",           masterAAtt,      *this);
    setupKnob(masterD,            "master_adsr_d",    "Decay",            masterDAtt,      *this);
    setupKnob(masterS,            "master_adsr_s",    "Sustain",          masterSAtt,      *this);
    setupKnob(masterR,            "master_adsr_r",    "Release",          masterRAtt,      *this);

    auto makeSmallLabel = [&](juce::Label& l, const juce::String& text, juce::Component& parent)
    {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, IndustrialTheme::accentMaster(), 10.f);
        parent.addAndMakeVisible(l);
    };
    const juce::String masterLabelTexts[] = {
        "VOL","TUNE","OCT","PAN","PORT","AGE","FEED","PB","MW","ATK","DEC","SUS","REL" };
    for (int i = 0; i < 13; ++i)
        makeSmallLabel(masterLabels[(size_t)i], masterLabelTexts[i], *this);

    applyLookAndFeel(*this);
}

MainPanel::~MainPanel()
{
    clearLookAndFeel(*this);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::buildOscillatorsTab()
{
    for (auto& tab : oscillatorTabs)
    {
        tab = new OscillatorPageComponent();
        tab->onPageResized = [this] { layoutOscillatorsTab(); };
    }

    auto makeId = [](const juce::String& base, int index)
    {
        return index == 0 ? base : base + juce::String(index + 1);
    };

    for (int i = 0; i < 8; ++i)
    {
        osc3dPanels[(size_t)i] = std::make_unique<OscPanel>(
            apvts, makeId("3d", i), "3D OSCILLATOR " + juce::String(i + 1));
        oscWTPanels[(size_t)i] = std::make_unique<OscPanel>(
            apvts, makeId("wt", i), "WAVETABLE " + juce::String(i + 1));
        ksPanels[(size_t)i] = std::make_unique<OscPanel>(
            apvts, makeId("ks", i), "KARPLUS-STRONG " + juce::String(i + 1));
        granularPanels[(size_t)i] = std::make_unique<OscPanel>(
            apvts, makeId("grain", i), "GRANULAR " + juce::String(i + 1),
            TerrainImportCallback{}, -1, granularImport, i, granularCapture);
        terrainPanels[(size_t)i] = std::make_unique<OscPanel>(
            apvts, makeId("terrain", i), "TERRAIN " + juce::String(i + 1),
            terrainImport, i);

        auto* parent = oscillatorTabs[(size_t) (i / 4)];
        parent->addAndMakeVisible(*osc3dPanels[(size_t)i]);
        parent->addAndMakeVisible(*oscWTPanels[(size_t)i]);
        parent->addAndMakeVisible(*ksPanels[(size_t)i]);
        parent->addAndMakeVisible(*granularPanels[(size_t)i]);
        parent->addAndMakeVisible(*terrainPanels[(size_t)i]);
    }

    static const juce::StringArray vaIds {
        "va","va2","va3","va4","va5","va6","va7","va8" };
    for (int i = 0; i < 8; ++i)
    {
        vaPanels[(size_t)i] = std::make_unique<OscPanel>(
            apvts, vaIds[i], "VIRTUAL ANALOG " + juce::String(i + 1));
        oscillatorTabs[(size_t) (i / 4)]->addAndMakeVisible(*vaPanels[(size_t)i]);
    }

    for (int i = 0; i < 2; ++i)
    {
        fmBlockViews[(size_t)i].engineIndex = i;
        oscillatorTabs[(size_t)i]->addAndMakeVisible(fmBlockViews[(size_t)i]);
    }

    // Engine slot bar — drives the layout
    for (int i = 0; i < 2; ++i)
    {
        slotBars[(size_t)i] = std::make_unique<EngineSlotBar>(
            apvts, [this] { layoutOscillatorsTab(); }, i);
        oscillatorTabs[(size_t)i]->addAndMakeVisible(*slotBars[(size_t)i]);
    }

    // Detune knobs (always shown — relevant whenever >1 3D or dual FM)
    auto makeEngLabel = [&](juce::Label& l, const juce::String& text)
    {
        l.setText(text, juce::dontSendNotification);
        IndustrialTheme::styleLabel(l, IndustrialTheme::accentMaster(), 9.f);
        addAndMakeVisible(l);
    };
    makeEngLabel(engDetuneLabels[0], "3D DTN");
    makeEngLabel(engDetuneLabels[1], "FM DTN");

    setupKnob(eng3dDetuneSlider, "engine_3d_detune", "3D Ensemble Detune",
              eng3dDetuneAtt, *this, IndustrialTheme::accentMaster());
    setupKnob(engFMDetuneSlider, "engine_fm_detune", "FM Dual Engine Detune (cents)",
              engFMDetuneAtt, *this, IndustrialTheme::accentMaster());

    tabs.addTab("OSC 1", kBg, oscillatorTabs[0], true);
    tabs.addTab("OSC 2", kBg, oscillatorTabs[1], true);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::buildEngineTab()
{
    tabs.addTab("FM A", kBg, new FMEngineEditor(apvts, 0), true);
    tabs.addTab("FM B", kBg, new FMEngineEditor(apvts, 1), true);
    engineTab = nullptr;
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::buildEngineFiltersTab()
{
    tabs.addTab("ENGINE FILTERS", kBg, new EngineFilterPage(apvts), true);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::buildEffectsTab()
{
    effectsTab = new juce::Component();

    filterPanel = std::make_unique<FilterPanel>(apvts);
    modPanel    = std::make_unique<ModMatrixPanel>(apvts);
    arpPanel    = std::make_unique<ArpPanel>(apvts);
    lfoPanel    = std::make_unique<LFOPanel>(apvts);

    effectsTab->addAndMakeVisible(*filterPanel);
    effectsTab->addAndMakeVisible(*modPanel);
    effectsTab->addAndMakeVisible(*arpPanel);
    effectsTab->addAndMakeVisible(*lfoPanel);

    tabs.addTab("FILTER / MOD / ARP", kBg, effectsTab, true);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::buildSequencerTab()
{
    sequencerTab = new juce::Component();
    modSequencerPanel = std::make_unique<ModStepSequencerPanel>(apvts, modStepProvider);
    sequencerTab->addAndMakeVisible(*modSequencerPanel);
    tabs.addTab("MOD SEQ", kBg, sequencerTab, true);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::applyLookAndFeel(juce::Component& c)
{
    c.setLookAndFeel(&lookAndFeel);
    for (int i = 0; i < c.getNumChildComponents(); ++i)
        applyLookAndFeel(*c.getChildComponent(i));
}

void MainPanel::clearLookAndFeel(juce::Component& c)
{
    c.setLookAndFeel(nullptr);
    for (int i = 0; i < c.getNumChildComponents(); ++i)
        clearLookAndFeel(*c.getChildComponent(i));
}

void MainPanel::setupKnob(juce::Slider& s, const juce::String& paramId,
                          const juce::String& tip, std::unique_ptr<Att>& att,
                          juce::Component& parent, juce::Colour accent)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 16);
    s.setTooltip(tip);
    IndustrialTheme::styleSlider(s, accent);
    parent.addAndMakeVisible(s);
    att = std::make_unique<Att>(apvts, paramId, s);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::paint(juce::Graphics& g)
{
    IndustrialTheme::paintFaceplate(g, getLocalBounds());
}

//──────────────────────────────────────────────────────────────────────────────
// Dynamically lays out the oscillator panels based on the current slot config.
// Called both from resized() and whenever the slot bar changes.
void MainPanel::layoutOscillatorsTab()
{
    if (!oscillatorTabs[0] || !oscillatorTabs[1]) return;

    // Hide all engine panels first
    for (auto& p : osc3dPanels) if (p) p->setVisible(false);
    for (auto& p : oscWTPanels) if (p) p->setVisible(false);
    for (auto& p : ksPanels) if (p) p->setVisible(false);
    for (auto& p : granularPanels) if (p) p->setVisible(false);
    for (auto& p : terrainPanels) if (p) p->setVisible(false);
    for (auto& p : vaPanels) if (p) p->setVisible(false);
    for (auto& v : fmBlockViews) v.setVisible(false);

    auto layoutPage = [&](int page)
    {
        auto* tab = oscillatorTabs[(size_t) page];
        auto* pageSlotBar = slotBars[(size_t) page].get();
        if (tab == nullptr || pageSlotBar == nullptr)
            return;

        auto tabArea = tab->getLocalBounds().reduced(6);

        const int kSlotBarH = 34;
        pageSlotBar->setBounds(tabArea.removeFromTop(kSlotBarH));
        tabArea.removeFromTop(8);

        struct PanelSlot { juce::Component* comp; int units; };
        std::vector<PanelSlot> panels;

        auto addPanel = [&](juce::Component* comp, int units = 1)
        {
            panels.push_back({comp, units});
        };

        auto addGap = [&] { panels.push_back({nullptr, 1}); };

        if (pageSlotBar->getBlockMode(page) == 1) // FM block
        {
            fmBlockViews[(size_t) page].setVisible(true);
            addPanel(&fmBlockViews[(size_t) page], 4);
        }
        else // Multi block — scan 4 slots for this OSC tab
        {
            const int block = page;
            for (int s = 0; s < 4; ++s)
            {
                int type = pageSlotBar->getSlotType(block, s);
                const int slotIndex = block * 4 + s;
                switch (type)
                {
                    case 1: // 3D
                        if (osc3dPanels[(size_t)slotIndex])
                        {
                            osc3dPanels[(size_t)slotIndex]->setVisible(true);
                            addPanel(osc3dPanels[(size_t)slotIndex].get());
                        }
                        break;
                    case 2: // VA
                        if (vaPanels[(size_t)slotIndex])
                        {
                            vaPanels[(size_t)slotIndex]->setVisible(true);
                            addPanel(vaPanels[(size_t)slotIndex].get());
                        }
                        break;
                    case 3: // WT
                        if (oscWTPanels[(size_t)slotIndex])
                        {
                            oscWTPanels[(size_t)slotIndex]->setVisible(true);
                            addPanel(oscWTPanels[(size_t)slotIndex].get());
                        }
                        break;
                    case 4: // KS
                        if (ksPanels[(size_t)slotIndex])
                        {
                            ksPanels[(size_t)slotIndex]->setVisible(true);
                            addPanel(ksPanels[(size_t)slotIndex].get());
                        }
                        break;
                    case 5: // Granular
                        if (granularPanels[(size_t)slotIndex])
                        {
                            granularPanels[(size_t)slotIndex]->setVisible(true);
                            addPanel(granularPanels[(size_t)slotIndex].get());
                        }
                        break;
                    case 6: // Terrain
                        if (terrainPanels[(size_t)slotIndex])
                        {
                            terrainPanels[(size_t)slotIndex]->setVisible(true);
                            addPanel(terrainPanels[(size_t)slotIndex].get(), s < 3 ? 2 : 1);
                        }
                        if (s < 3)
                            ++s;
                        break;
                    default:
                        addGap();
                        break;
                }
            }
        }

        auto layoutWide = [&]
        {
            const float unitW = (float)tabArea.getWidth() / 4.f;
            float x = (float)tabArea.getX();
            for (auto& p : panels)
            {
                int w = juce::roundToInt((float)p.units * unitW);
                if (p.comp != nullptr)
                    p.comp->setBounds(juce::roundToInt(x), tabArea.getY(), w, tabArea.getHeight());
                x += (float)p.units * unitW;
            }
        };

        auto layoutCompact = [&]
        {
            const int gap = 4;
            const int columns = 2;
            const int cellW = (tabArea.getWidth() - gap) / columns;
            const int rowH  = (tabArea.getHeight() - gap) / 2;
            int row = 0;
            int col = 0;

            auto advance = [&](int span)
            {
                col += span;
                if (col >= columns)
                {
                    ++row;
                    col = 0;
                }
            };

            for (auto& p : panels)
            {
                int span = juce::jlimit(1, columns, p.units);
                if (col + span > columns)
                {
                    ++row;
                    col = 0;
                }

                if (p.comp != nullptr)
                {
                    auto r = juce::Rectangle<int>(
                        tabArea.getX() + col * (cellW + gap),
                        tabArea.getY() + row * (rowH + gap),
                        cellW * span + gap * (span - 1),
                        rowH);
                    p.comp->setBounds(r);
                }

                advance(span);
            }
        };

        const bool compact = tabArea.getWidth() < 980 && panels.size() > 2
                             && pageSlotBar->getBlockMode(page) == 0;
        compact ? layoutCompact() : layoutWide();
    };

    layoutPage(0);
    layoutPage(1);
}

//──────────────────────────────────────────────────────────────────────────────
void MainPanel::resized()
{
    auto area = getLocalBounds();

    // Master strip at bottom
    auto masterStrip = area.removeFromBottom(124);
    masterLabel.setBounds(masterStrip.removeFromLeft(70));

    auto masterLabelRow = masterStrip.removeFromTop(14);
    auto masterKnobRow  = masterStrip;

    auto positionMaster = [&](juce::Label& lbl, juce::Slider& knob, int w)
    {
        lbl .setBounds(masterLabelRow.removeFromLeft(w));
        knob.setBounds(masterKnobRow .removeFromLeft(w));
    };

    positionMaster(masterLabels[0], masterVolSlider,    60);
    positionMaster(masterLabels[1], masterTuneSlider,   60);
    positionMaster(masterLabels[2], masterOctaveSlider, 60);
    positionMaster(masterLabels[3], masterPanSlider,    60);
    positionMaster(masterLabels[4], masterPortaSlider,  60);
    positionMaster(masterLabels[5], masterAgeSlider,    60);
    positionMaster(masterLabels[6], masterFeedSlider,   60);
    positionMaster(masterLabels[7], masterPBSlider,     60);
    positionMaster(masterLabels[8], masterMWSlider,     60);
    masterLabelRow.removeFromLeft(8);
    masterKnobRow .removeFromLeft(8);
    positionMaster(masterLabels[9],  masterA, 60);
    positionMaster(masterLabels[10], masterD, 60);
    positionMaster(masterLabels[11], masterS, 60);
    positionMaster(masterLabels[12], masterR, 60);
    masterLabelRow.removeFromLeft(10);
    masterKnobRow .removeFromLeft(10);
    positionMaster(engDetuneLabels[0], eng3dDetuneSlider, 60);
    positionMaster(engDetuneLabels[1], engFMDetuneSlider, 60);

    tabs.setBounds(area);

    // ── Oscillators tab ───────────────────────────────────────────────────────
    layoutOscillatorsTab();

    // ── FM ENGINE tab ─────────────────────────────────────────────────────────
    if (engineTab)
    {
        auto tabArea = engineTab->getLocalBounds().reduced(8);
        const int opW = juce::jmax(54, tabArea.getWidth() / 10);

        {
            auto titleRow = tabArea.removeFromTop(20);
            fmEnableButton.setBounds(titleRow.removeFromRight(48));
            fmEnableLabel .setBounds(titleRow.removeFromLeft(160));
        }
        tabArea.removeFromTop(4);

        auto topLabelRow = tabArea.removeFromTop(13);
        fmAlgoLabel.setBounds(topLabelRow.removeFromLeft(180));
        fmLvlLabel .setBounds(topLabelRow.removeFromLeft(80));
        fmOctLabel .setBounds(topLabelRow.removeFromLeft(80));
        topLabelRow.removeFromLeft(8);
        for (auto& label : fmAdsrLabels)
            label.setBounds(topLabelRow.removeFromLeft(48));

        auto topRow = tabArea.removeFromTop(84);
        fmAlgorithmBox.setBounds(topRow.removeFromLeft(180).withHeight(24));
        fmLevelSlider .setBounds(topRow.removeFromLeft(80));
        fmOctaveSlider.setBounds(topRow.removeFromLeft(80));
        topRow.removeFromLeft(8);
        fmA.setBounds(topRow.removeFromLeft(48).reduced(2, 0));
        fmD.setBounds(topRow.removeFromLeft(48).reduced(2, 0));
        fmS.setBounds(topRow.removeFromLeft(48).reduced(2, 0));
        fmR.setBounds(topRow.removeFromLeft(48).reduced(2, 0));

        tabArea.removeFromTop(6);

        const int labelH = 10;
        const int gap    = 2;
        const int waveH  = 22;
        const int sliderRows = 9;
        const int sliderH = juce::jlimit(18, 54,
            (tabArea.getHeight() - 13 - labelH - waveH - sliderRows * labelH - (sliderRows + 1) * gap)
                / sliderRows);

        auto opIndexRow = tabArea.removeFromTop(13);
        for (int i = 0; i < 10; ++i)
            fmOpLabels[(size_t)i].setBounds(opIndexRow.removeFromLeft(opW));

        auto layoutSliderRow = [&](juce::Label& header, auto& sliders)
        {
            header.setBounds(tabArea.removeFromTop(labelH));
            auto row = tabArea.removeFromTop(sliderH);
            for (int i = 0; i < 10; ++i)
                sliders[(size_t)i].setBounds(row.removeFromLeft(opW).reduced(2));
            tabArea.removeFromTop(gap);
        };

        layoutSliderRow(fmCoarseHdrLabel,  fmOpCoarseSliders);
        layoutSliderRow(fmFineHdrLabel,    fmOpFineSliders);
        layoutSliderRow(fmDetuneHdrLabel,  fmOpDetuneSliders);
        layoutSliderRow(fmLevelHdrLabel,   fmOpLevelSliders);
        layoutSliderRow(fmFeedbackHdrLabel,fmOpFeedbackSliders);

        fmWaveHdrLabel.setBounds(tabArea.removeFromTop(labelH));
        auto waveRow = tabArea.removeFromTop(waveH);
        for (int i = 0; i < 10; ++i)
            fmOpWaveBoxes[(size_t)i].setBounds(waveRow.removeFromLeft(opW).reduced(2, 0));
        tabArea.removeFromTop(gap);

        layoutSliderRow(fmSkirtHdrLabel, fmOpSkirtSliders);
        layoutSliderRow(fmResHdrLabel,   fmOpResSliders);
        layoutSliderRow(fmVelHdrLabel,   fmOpVelSliders);
        layoutSliderRow(fmKeyHdrLabel,   fmOpKeySliders);
    }

    // ── Filter / Mod / Arp tab ────────────────────────────────────────────────
    if (effectsTab && filterPanel && modPanel && arpPanel && lfoPanel)
    {
        auto tabArea = effectsTab->getLocalBounds().reduced(6);
        filterPanel->setBounds(tabArea.removeFromTop(240).reduced(3));
        tabArea.removeFromTop(4);
        int arpW = 240, lfoW = 280;
        arpPanel->setBounds(tabArea.removeFromRight(arpW).reduced(3));
        lfoPanel->setBounds(tabArea.removeFromRight(lfoW).reduced(3));
        modPanel->setBounds(tabArea.reduced(3));
    }

    if (sequencerTab && modSequencerPanel)
        modSequencerPanel->setBounds(sequencerTab->getLocalBounds().reduced(6));
}
