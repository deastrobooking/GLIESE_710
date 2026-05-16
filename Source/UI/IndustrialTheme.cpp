#include "IndustrialTheme.h"

namespace IndustrialTheme
{
namespace
{
constexpr juce::uint32 kBackground = 0xff050607;
constexpr juce::uint32 kPanelBase  = 0xff0b0e10;
constexpr juce::uint32 kPanelEdge  = 0xff20272b;
constexpr juce::uint32 kText       = 0xffe9fff5;
constexpr juce::uint32 kMutedText  = 0xff7f8d88;

juce::Colour glow(juce::Colour c, float alpha)
{
    return c.withAlpha(alpha);
}

unsigned hashNoise(int x, int y)
{
    unsigned n = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    n = (n ^ (n >> 13u)) * 1274126177u;
    return n ^ (n >> 16u);
}

void paintNoise(juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour tint, float alpha)
{
    g.setColour(tint.withAlpha(alpha));
    for (int y = bounds.getY(); y < bounds.getBottom(); y += 5)
    {
        for (int x = bounds.getX(); x < bounds.getRight(); x += 7)
        {
            if ((hashNoise(x, y) & 7u) == 0u)
                g.fillRect(x, y, 1, 1);
        }
    }
}

void drawScrew(juce::Graphics& g, juce::Point<float> centre, juce::Colour accent)
{
    auto r = juce::Rectangle<float>(centre.x - 4.f, centre.y - 4.f, 8.f, 8.f);
    g.setColour(juce::Colour(0xff020303));
    g.fillEllipse(r);
    g.setColour(juce::Colour(0xff333a3d));
    g.drawEllipse(r, 1.f);
    g.setColour(accent.withAlpha(0.45f));
    g.drawLine(centre.x - 2.5f, centre.y, centre.x + 2.5f, centre.y, 1.f);
}
}

juce::Colour background()   { return juce::Colour(kBackground); }
juce::Colour panelBase()    { return juce::Colour(kPanelBase); }
juce::Colour panelEdge()    { return juce::Colour(kPanelEdge); }
juce::Colour text()         { return juce::Colour(kText); }
juce::Colour mutedText()    { return juce::Colour(kMutedText); }
juce::Colour accentMaster() { return juce::Colour(0xffff3b2f); }
juce::Colour accentFM()     { return juce::Colour(0xffff2bd6); }
juce::Colour accentFilter() { return juce::Colour(0xffff9f1c); }
juce::Colour accentMod()    { return juce::Colour(0xffb06cff); }
juce::Colour accentArp()    { return juce::Colour(0xffffd23f); }

juce::Colour accentLFO(int index)
{
    static const juce::Colour accents[] = {
        juce::Colour(0xff39ff14),
        juce::Colour(0xffff7a18),
        juce::Colour(0xffff2bd6),
        juce::Colour(0xffc7ff38)
    };
    return accents[(size_t) juce::jlimit(0, 3, index)];
}

juce::Colour accentForOsc(const juce::String& oscId)
{
    if (oscId.startsWith("3d")) return juce::Colour(0xff39ff14);
    if (oscId == "va")  return juce::Colour(0xffff2bd6);
    if (oscId == "va2") return juce::Colour(0xff00e5ff);
    if (oscId == "va3") return juce::Colour(0xfffff000);
    if (oscId == "va4") return juce::Colour(0xffff6a00);
    if (oscId.startsWith("va")) return juce::Colour(0xff00e5ff);
    if (oscId.startsWith("wt")) return juce::Colour(0xffffb000);
    if (oscId.startsWith("ks")) return juce::Colour(0xff00ffcc);
    if (oscId.startsWith("grain")) return juce::Colour(0xff7dff7a);
    if (oscId.startsWith("terrain")) return juce::Colour(0xff7aa6ff);
    return accentMaster();
}

void styleTitle(juce::Label& label, juce::Colour accent, float size)
{
    label.setColour(juce::Label::textColourId, text());
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setFont(juce::Font(juce::FontOptions { size, juce::Font::bold }));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setMinimumHorizontalScale(0.75f);
    label.setTooltip(label.getText());
    label.setColour(juce::Label::backgroundColourId, accent.withAlpha(0.03f));
}

void styleLabel(juce::Label& label, juce::Colour accent, float size,
                juce::Justification justification)
{
    label.setColour(juce::Label::textColourId, accent.brighter(0.18f));
    label.setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label.setFont(juce::Font(juce::FontOptions { size, juce::Font::bold }));
    label.setJustificationType(justification);
    label.setMinimumHorizontalScale(0.65f);
}

void styleSlider(juce::Slider& slider, juce::Colour accent)
{
    slider.setColour(juce::Slider::thumbColourId, accent);
    slider.setColour(juce::Slider::trackColourId, accent);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff161d20));
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff050708));
    slider.setColour(juce::Slider::textBoxTextColourId, text());
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff050708));
    slider.setColour(juce::Slider::textBoxOutlineColourId, accent.withAlpha(0.35f));
    slider.setColour(juce::Slider::textBoxHighlightColourId, accent.withAlpha(0.4f));
}

void styleCombo(juce::ComboBox& combo, juce::Colour accent)
{
    combo.setJustificationType(juce::Justification::centred);
    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff050708));
    combo.setColour(juce::ComboBox::outlineColourId, accent.withAlpha(0.55f));
    combo.setColour(juce::ComboBox::textColourId, text());
    combo.setColour(juce::ComboBox::arrowColourId, accent);
    combo.setColour(juce::ComboBox::buttonColourId, juce::Colour(0xff0e1315));
}

void styleButton(juce::Button& button, juce::Colour accent)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff07090a));
    button.setColour(juce::TextButton::buttonOnColourId, accent.withAlpha(0.45f));
    button.setColour(juce::TextButton::textColourOffId, text());
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    button.setColour(juce::ToggleButton::textColourId, text());
    button.setColour(juce::ToggleButton::tickColourId, accent);
    button.setColour(juce::ToggleButton::tickDisabledColourId, mutedText());
}

void paintFaceplate(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::ColourGradient gradient(juce::Colour(0xff020303), 0.f, 0.f,
                                  juce::Colour(0xff0c1011), 0.f,
                                  (float)bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRect(bounds);

    g.setColour(juce::Colour(0xff151b1e));
    for (int y = bounds.getY() + 22; y < bounds.getBottom(); y += 38)
        g.drawLine((float)bounds.getX(), (float)y, (float)bounds.getRight(), (float)y, 0.45f);

    g.setColour(juce::Colour(0xff101517));
    for (int x = bounds.getX() + 28; x < bounds.getRight(); x += 56)
        g.drawLine((float)x, (float)bounds.getY(), (float)x, (float)bounds.getBottom(), 0.35f);

    auto red = accentMaster();
    g.setColour(red.withAlpha(0.18f));
    for (int x = -80; x < bounds.getRight(); x += 210)
        g.drawLine((float)x, (float)bounds.getBottom(), (float)(x + 150), 0.f, 1.f);

    g.setColour(juce::Colour(0xff22292c));
    g.drawRect(bounds.reduced(2), 1);

    paintNoise(g, bounds, juce::Colours::white, 0.028f);
    paintNoise(g, bounds, red, 0.018f);
}

void paintPanel(juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour accent,
                bool denseLines)
{
    auto r = bounds.toFloat().reduced(0.5f);
    g.setColour(panelBase());
    g.fillRoundedRectangle(r, 5.f);

    g.setColour(juce::Colour(0xff040506));
    g.fillRoundedRectangle(r.reduced(3.f), 3.f);

    g.setColour(panelEdge());
    g.drawRoundedRectangle(r, 5.f, 1.f);
    g.setColour(accent.withAlpha(0.72f));
    g.drawRoundedRectangle(r.reduced(1.f), 4.f, 1.1f);

    auto inner = bounds.reduced(8);
    g.setColour(accent.withAlpha(denseLines ? 0.12f : 0.08f));
    const int step = denseLines ? 12 : 18;
    for (int y = inner.getY() + step; y < inner.getBottom(); y += step)
        g.drawLine((float)inner.getX(), (float)y, (float)inner.getRight(), (float)y, 0.55f);

    g.setColour(juce::Colour(0xff1c2426));
    for (int x = inner.getX() + 24; x < inner.getRight(); x += 48)
        g.drawLine((float)x, (float)inner.getY(), (float)x, (float)inner.getBottom(), 0.35f);

    paintNoise(g, bounds.reduced(3), juce::Colours::white, 0.025f);
    paintNoise(g, bounds.reduced(3), accent, 0.024f);

    drawScrew(g, r.getTopLeft() + juce::Point<float>(10.f, 10.f), accent);
    drawScrew(g, r.getTopRight() + juce::Point<float>(-10.f, 10.f), accent);
    drawScrew(g, r.getBottomLeft() + juce::Point<float>(10.f, -10.f), accent);
    drawScrew(g, r.getBottomRight() + juce::Point<float>(-10.f, -10.f), accent);
}

IndustrialLookAndFeel::IndustrialLookAndFeel()
{
    setColour(juce::Label::textColourId, text());
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff050708));
    setColour(juce::PopupMenu::textColourId, text());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accentMaster().withAlpha(0.35f));
    setColour(juce::PopupMenu::highlightedTextColourId, text());
}

void IndustrialLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPosProportional,
                                             float rotaryStartAngle,
                                             float rotaryEndAngle,
                                             juce::Slider& slider)
{
    auto accent = slider.findColour(juce::Slider::thumbColourId);
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(6.f);
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto radius = size * 0.5f;
    auto centre = bounds.getCentre();
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    for (int i = 4; i > 0; --i)
    {
        g.setColour(glow(accent, 0.035f * (float)i));
        g.drawEllipse(juce::Rectangle<float>(centre.x - radius - (float)i,
                                             centre.y - radius - (float)i,
                                             radius * 2.f + (float)i * 2.f,
                                             radius * 2.f + (float)i * 2.f), 1.f);
    }

    g.setColour(juce::Colour(0xff030404));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.f, radius * 2.f);
    g.setColour(juce::Colour(0xff171f22));
    g.fillEllipse(centre.x - radius * 0.84f, centre.y - radius * 0.84f,
                  radius * 1.68f, radius * 1.68f);

    juce::Path baseArc;
    baseArc.addCentredArc(centre.x, centre.y, radius * 0.92f, radius * 0.92f,
                          0.f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff273033));
    g.strokePath(baseArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius * 0.92f, radius * 0.92f,
                           0.f, rotaryStartAngle, angle, true);
    g.setColour(accent.withAlpha(0.9f));
    g.strokePath(valueArc, juce::PathStrokeType(3.4f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    for (int i = 0; i <= 10; ++i)
    {
        auto tickAngle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * (float)i / 10.f;
        auto p1 = centre.getPointOnCircumference(radius * 1.03f, tickAngle);
        auto p2 = centre.getPointOnCircumference(radius * 1.13f, tickAngle);
        g.setColour((i % 5 == 0 ? accent : mutedText()).withAlpha(i % 5 == 0 ? 0.65f : 0.28f));
        g.drawLine({ p1, p2 }, i % 5 == 0 ? 1.2f : 0.8f);
    }

    juce::Path pointer;
    pointer.addRoundedRectangle(-1.5f, -radius * 0.72f, 3.f, radius * 0.43f, 1.3f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
    g.setColour(accent);
    g.fillPath(pointer);

    g.setColour(juce::Colour(0xff050708));
    g.fillEllipse(centre.x - radius * 0.28f, centre.y - radius * 0.28f,
                  radius * 0.56f, radius * 0.56f);
    g.setColour(accent.withAlpha(0.45f));
    g.drawEllipse(centre.x - radius * 0.28f, centre.y - radius * 0.28f,
                  radius * 0.56f, radius * 0.56f, 1.f);
}

void IndustrialLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float minSliderPos, float maxSliderPos,
                                             const juce::Slider::SliderStyle style,
                                             juce::Slider& slider)
{
    auto accent = slider.findColour(juce::Slider::thumbColourId);
    auto area = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(4.f);

    if (style == juce::Slider::LinearVertical)
    {
        auto slot = juce::Rectangle<float>(area.getCentreX() - 3.f, area.getY() + 4.f,
                                           6.f, area.getHeight() - 8.f);
        g.setColour(juce::Colour(0xff030404));
        g.fillRoundedRectangle(slot.expanded(2.f, 0.f), 3.f);
        g.setColour(juce::Colour(0xff273033));
        g.drawRoundedRectangle(slot.expanded(2.f, 0.f), 3.f, 1.f);

        auto fill = slot.withY(juce::jlimit(slot.getY(), slot.getBottom(), sliderPos))
                        .withBottom(slot.getBottom());
        g.setColour(accent.withAlpha(0.28f));
        g.fillRoundedRectangle(fill.expanded(5.f, 0.f), 4.f);
        g.setColour(accent);
        g.fillRoundedRectangle(fill, 3.f);

        auto handle = juce::Rectangle<float>(area.getX() + 4.f, sliderPos - 5.f,
                                             area.getWidth() - 8.f, 10.f);
        g.setColour(juce::Colour(0xff050708));
        g.fillRoundedRectangle(handle, 3.f);
        g.setColour(accent.withAlpha(0.95f));
        g.drawRoundedRectangle(handle.reduced(0.5f), 3.f, 1.2f);
        g.drawLine(handle.getX() + 4.f, sliderPos, handle.getRight() - 4.f, sliderPos, 1.f);
        return;
    }

    auto slot = juce::Rectangle<float>(area.getX(), area.getCentreY() - 3.f,
                                       area.getWidth(), 6.f);
    g.setColour(juce::Colour(0xff030404));
    g.fillRoundedRectangle(slot.expanded(0.f, 2.f), 3.f);
    g.setColour(juce::Colour(0xff273033));
    g.drawRoundedRectangle(slot.expanded(0.f, 2.f), 3.f, 1.f);

    auto centre = (minSliderPos + maxSliderPos) * 0.5f;
    auto left = juce::jmin(centre, sliderPos);
    auto right = juce::jmax(centre, sliderPos);
    g.setColour(accent.withAlpha(0.85f));
    g.fillRoundedRectangle(juce::Rectangle<float>(left, slot.getY(), right - left, slot.getHeight()), 3.f);

    auto handle = juce::Rectangle<float>(sliderPos - 4.5f, area.getCentreY() - 10.f, 9.f, 20.f);
    g.setColour(juce::Colour(0xff050708));
    g.fillRoundedRectangle(handle, 2.f);
    g.setColour(accent);
    g.drawRoundedRectangle(handle.reduced(0.5f), 2.f, 1.1f);
}

void IndustrialLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                         int buttonX, int buttonY, int buttonW, int buttonH,
                                         juce::ComboBox& box)
{
    auto accent = box.findColour(juce::ComboBox::outlineColourId);
    auto bounds = juce::Rectangle<float>(0.f, 0.f, (float)width, (float)height).reduced(0.5f);
    g.setColour(juce::Colour(0xff050708));
    g.fillRoundedRectangle(bounds, 3.f);
    g.setColour(isButtonDown ? accent.withAlpha(0.9f) : accent.withAlpha(0.62f));
    g.drawRoundedRectangle(bounds, 3.f, 1.f);

    auto arrow = juce::Rectangle<float>((float)buttonX, (float)buttonY,
                                        (float)buttonW, (float)buttonH).reduced(7.f, 7.f);
    juce::Path p;
    p.addTriangle(arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(),
                  arrow.getCentreX(), arrow.getBottom());
    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(p);
}

void IndustrialLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(5, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont(juce::Font(juce::FontOptions { 11.f, juce::Font::bold }));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, text());
}

void IndustrialLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool shouldDrawButtonAsHighlighted,
                                                 bool shouldDrawButtonAsDown)
{
    auto accent = button.findColour(juce::TextButton::buttonOnColourId).withAlpha(1.f);
    if (accent == juce::Colours::transparentBlack)
        accent = backgroundColour;

    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff050708));
    g.fillRoundedRectangle(bounds, 3.f);

    auto alpha = button.getToggleState() || shouldDrawButtonAsDown ? 0.85f
               : shouldDrawButtonAsHighlighted ? 0.55f : 0.32f;
    g.setColour(accent.withAlpha(alpha));
    g.drawRoundedRectangle(bounds, 3.f, 1.f);
    if (button.getToggleState())
    {
        g.setColour(accent.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds.reduced(2.f), 2.f);
    }
}

void IndustrialLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                           bool, bool)
{
    g.setColour(button.findColour(button.getToggleState()
                                      ? juce::TextButton::textColourOnId
                                      : juce::TextButton::textColourOffId));
    g.setFont(juce::Font(juce::FontOptions { 11.f, juce::Font::bold }));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(3),
                     juce::Justification::centred, 1);
}

void IndustrialLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    auto accent = button.findColour(juce::ToggleButton::tickColourId);
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto box = bounds.removeFromLeft(juce::jmin(22.f, bounds.getHeight())).reduced(3.f);

    g.setColour(juce::Colour(0xff050708));
    g.fillRoundedRectangle(box, 3.f);

    auto borderAlpha = button.getToggleState() || shouldDrawButtonAsDown ? 0.95f
                    : shouldDrawButtonAsHighlighted ? 0.62f : 0.4f;
    g.setColour(accent.withAlpha(borderAlpha));
    g.drawRoundedRectangle(box, 3.f, 1.f);

    if (button.getToggleState())
    {
        g.setColour(accent.withAlpha(0.25f));
        g.fillRoundedRectangle(box.reduced(2.f), 2.f);
        g.setColour(accent);
        g.drawLine(box.getX() + 4.f, box.getCentreY(),
                   box.getCentreX(), box.getBottom() - 4.f, 1.8f);
        g.drawLine(box.getCentreX(), box.getBottom() - 4.f,
                   box.getRight() - 4.f, box.getY() + 4.f, 1.8f);
    }

    g.setColour(text());
    g.setFont(juce::Font(juce::FontOptions { 10.f, juce::Font::bold }));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().withLeft((int)box.getRight() + 3),
                     juce::Justification::centredLeft, 1);
}
}
