#include "EngineSlotBar.h"

static auto blockParamId(int b)       { return juce::String("engine_block_") + (b == 0 ? "a" : "b") + "_mode"; }
static auto slotParamId (int b, int s){ return juce::String("engine_block_") + (b == 0 ? "a" : "b") + "_slot_" + juce::String(s); }

//──────────────────────────────────────────────────────────────────────────────
const char* EngineSlotBar::typeName(int t) noexcept
{
    switch (t) {
        case Osc3D: return "3D";
        case VA:    return "VA";
        case WT:    return "WT";
        case KS:    return "KS";
        case Granular: return "Granular";
        case Terrain: return "Terrain";
        default:    return "—";
    }
}

juce::Colour EngineSlotBar::accentFor(int t) noexcept
{
    switch (t) {
        case Osc3D: return IndustrialTheme::accentForOsc("3d");
        case VA:    return IndustrialTheme::accentForOsc("va");
        case WT:    return IndustrialTheme::accentForOsc("wt");
        case KS:    return IndustrialTheme::accentForOsc("ks");
        case Granular: return IndustrialTheme::accentForOsc("grain");
        case Terrain: return IndustrialTheme::accentForOsc("terrain");
        default:    return juce::Colour(0xff3a3e44);
    }
}

//──────────────────────────────────────────────────────────────────────────────
EngineSlotBar::EngineSlotBar(juce::AudioProcessorValueTreeState& a,
                             std::function<void()> cb)
    : apvts(a), onChanged(std::move(cb))
{
    for (int b = 0; b < kNumBlocks; ++b)
    {
        modePtr[b] = apvts.getRawParameterValue(blockParamId(b));
        apvts.addParameterListener(blockParamId(b), this);
        for (int s = 0; s < kSlotsPerBlock; ++s)
        {
            slotPtr[b][s] = apvts.getRawParameterValue(slotParamId(b, s));
            apvts.addParameterListener(slotParamId(b, s), this);
        }
    }
}

EngineSlotBar::~EngineSlotBar()
{
    for (int b = 0; b < kNumBlocks; ++b)
    {
        apvts.removeParameterListener(blockParamId(b), this);
        for (int s = 0; s < kSlotsPerBlock; ++s)
            apvts.removeParameterListener(slotParamId(b, s), this);
    }
}

void EngineSlotBar::parameterChanged(const juce::String&, float)
{
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<EngineSlotBar>(this)] {
        if (safeThis)
        {
            safeThis->repaint();
            if (safeThis->onChanged) safeThis->onChanged();
        }
    });
}

//──────────────────────────────────────────────────────────────────────────────
int EngineSlotBar::getBlockMode(int block) const
{
    juce::ignoreUnused(block);

    auto* a = modePtr[0];
    auto* b = modePtr[1];
    const bool pageAIsFM = a != nullptr && (int)a->load(std::memory_order_relaxed) == 1;
    const bool pageBIsFM = b != nullptr && (int)b->load(std::memory_order_relaxed) == 1;
    return pageAIsFM && pageBIsFM ? 1 : 0;
}

int EngineSlotBar::getSlotType(int block, int slot) const
{
    auto* p = slotPtr[block][slot];
    return p ? (int)p->load(std::memory_order_relaxed) : 0;
}

//──────────────────────────────────────────────────────────────────────────────
juce::Rectangle<int> EngineSlotBar::blockBounds(int block) const
{
    auto b = getLocalBounds();
    int  w = b.getWidth() / 2;
    return block == 0 ? b.removeFromLeft(w) : b.removeFromRight(w);
}

juce::Rectangle<int> EngineSlotBar::modeBtnBounds(int block) const
{
    return blockBounds(block).removeFromRight(44).reduced(2);
}

juce::Rectangle<int> EngineSlotBar::slotBounds(int block, int slot) const
{
    auto br = blockBounds(block);
    br.removeFromRight(46); // leave space for mode button + gap
    int w = br.getWidth() / kSlotsPerBlock;
    return br.withLeft(br.getX() + slot * w).withWidth(w).reduced(2);
}

//──────────────────────────────────────────────────────────────────────────────
void EngineSlotBar::paint(juce::Graphics& g)
{
    const juce::Colour fmAccent = IndustrialTheme::accentFM();

    for (int b = 0; b < kNumBlocks; ++b)
    {
        bool isFM = getBlockMode(b) == 1;
        auto br   = blockBounds(b).reduced(2);

        // Block background + border
        g.setColour(isFM ? fmAccent.withAlpha(0.07f) : juce::Colour(0xff0e1012));
        g.fillRoundedRectangle(br.toFloat(), 5.f);
        g.setColour(isFM ? fmAccent.withAlpha(0.45f) : IndustrialTheme::panelEdge());
        g.drawRoundedRectangle(br.toFloat(), 5.f, 1.f);

        // FM mode badge
        auto modeBtn = modeBtnBounds(b);
        g.setColour(isFM ? fmAccent : juce::Colour(0xff252830));
        g.fillRoundedRectangle(modeBtn.toFloat(), 3.f);
        g.setColour(isFM ? juce::Colour(0xff000000) : IndustrialTheme::mutedText());
        g.setFont(juce::Font(juce::FontOptions { 10.f, juce::Font::bold }));
        g.drawText("FM", modeBtn, juce::Justification::centred);

        if (isFM)
        {
            // Show FM engine label spanning the block
            auto labelArea = blockBounds(b).reduced(4);
            labelArea.removeFromRight(50);
            g.setColour(fmAccent);
            g.setFont(juce::Font(juce::FontOptions { 13.f, juce::Font::bold }));
            g.drawText("FM ENGINE " + juce::String(b + 1), labelArea,
                       juce::Justification::centred);
        }
        else
        {
            // Draw 4 slot units. Terrain spans two units because it owns a surface viewer.
            for (int s = 0; s < kSlotsPerBlock; ++s)
            {
                int  type = getSlotType(b, s);
                auto sr   = slotBounds(b, s);
                const bool spansTwo = type == Terrain && s < kSlotsPerBlock - 1;
                if (spansTwo)
                    sr = sr.getUnion(slotBounds(b, s + 1));
                auto ac   = accentFor(type);

                g.setColour(type != None ? ac.withAlpha(0.14f) : juce::Colour(0xff0d0f11));
                g.fillRoundedRectangle(sr.toFloat(), 3.f);
                g.setColour(type != None ? ac.withAlpha(0.65f) : IndustrialTheme::panelEdge());
                g.drawRoundedRectangle(sr.toFloat(), 3.f, 1.f);

                g.setColour(type != None ? ac : IndustrialTheme::mutedText());
                g.setFont(juce::Font(juce::FontOptions { 11.f, juce::Font::bold }));
                g.drawText(typeName(type), sr, juce::Justification::centred);

                if (spansTwo)
                    ++s;
            }
        }
    }
}

//──────────────────────────────────────────────────────────────────────────────
void EngineSlotBar::mouseDown(const juce::MouseEvent& e)
{
    for (int b = 0; b < kNumBlocks; ++b)
    {
        if (modeBtnBounds(b).contains(e.position.toInt()))
        {
            setBlockMode(b, getBlockMode(b) == 1 ? 0 : 1);
            return;
        }
        if (getBlockMode(b) == 0)  // Multi — handle slot clicks
        {
            for (int s = 0; s < kSlotsPerBlock; ++s)
            {
                auto target = slotBounds(b, s);
                const bool spansTwo = getSlotType(b, s) == Terrain && s < kSlotsPerBlock - 1;
                if (spansTwo)
                    target = target.getUnion(slotBounds(b, s + 1));

                if (target.contains(e.position.toInt()))
                {
                    showSlotMenu(b, s);
                    return;
                }

                if (spansTwo)
                    ++s;
            }
        }
    }
}

void EngineSlotBar::showSlotMenu(int block, int slot)
{
    juce::PopupMenu menu;
    int cur = getSlotType(block, slot);
    menu.addItem(1, "— None",           true, cur == None);
    menu.addItem(2, "3D Oscillator",    true, cur == Osc3D);
    menu.addItem(3, "Virtual Analog",   true, cur == VA);
    menu.addItem(4, "Wavetable",        true, cur == WT);
    menu.addItem(5, "Karplus-Strong",   true, cur == KS);
    menu.addSeparator();
    menu.addItem(6, "Granular Oscillator",     true, cur == Granular);
    menu.addItem(7, "Terrain Oscillator",      true, cur == Terrain);

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(this),
        [this, block, slot](int result) {
            if (result > 0) setSlotType(block, slot, result - 1);
        });
}

void EngineSlotBar::setBlockMode(int block, int m)
{
    juce::ignoreUnused(block);
    setAllBlockModes(m);
}

void EngineSlotBar::setAllBlockModes(int m)
{
    for (int b = 0; b < kNumBlocks; ++b)
    {
        if (auto* param = apvts.getParameter(blockParamId(b)))
            param->setValueNotifyingHost(param->convertTo0to1((float)m));
    }
}

void EngineSlotBar::setSlotType(int block, int slot, int t)
{
    if (auto* param = apvts.getParameter(slotParamId(block, slot)))
        param->setValueNotifyingHost(param->convertTo0to1((float)t));

    if (t == Terrain && slot < kSlotsPerBlock - 1)
    {
        if (auto* next = apvts.getParameter(slotParamId(block, slot + 1)))
            next->setValueNotifyingHost(next->convertTo0to1((float)None));
    }
}
