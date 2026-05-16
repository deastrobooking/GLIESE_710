#pragma once
#include <JuceHeader.h>
#include "IndustrialTheme.h"

/**
 * Two engine blocks (A = left, B = right), each spanning 4 "slot units".
 *
 * Each block has a clickable "FM" badge on its right edge that toggles the
 * block between FM mode (one FM engine, full-block width) and Multi mode
 * (up to 4 individual engine slots, each selectable as None/3D/VA/WT/KS).
 *
 * APVTS params owned:
 *   engine_block_a_mode  /  engine_block_b_mode   (0=Multi 1=FM)
 *   engine_block_a_slot_0..3  /  engine_block_b_slot_0..3  (0-4)
 */
class EngineSlotBar : public juce::Component,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int kNumBlocks     = 2;
    static constexpr int kSlotsPerBlock = 4;

    // Multi-mode slot types
    enum SlotType : int { None = 0, Osc3D = 1, VA = 2, WT = 3, KS = 4 };

    static const char*    typeName  (int t) noexcept;
    static juce::Colour   accentFor (int t) noexcept;

    EngineSlotBar(juce::AudioProcessorValueTreeState& apvts,
                  std::function<void()> onChanged);
    ~EngineSlotBar() override;

    void paint   (juce::Graphics&)           override;
    void resized ()                           override {}
    void mouseDown(const juce::MouseEvent&)  override;

    int getBlockMode (int block)              const;   // 0=Multi 1=FM
    int getSlotType  (int block, int slot)    const;   // 0–4

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::function<void()>               onChanged;

    std::atomic<float>* modePtr[kNumBlocks]                       {};
    std::atomic<float>* slotPtr[kNumBlocks][kSlotsPerBlock]       {};

    void parameterChanged(const juce::String&, float) override;

    void showSlotMenu (int block, int slot);
    void setBlockMode (int block, int m);
    void setSlotType  (int block, int slot, int t);

    juce::Rectangle<int> blockBounds   (int block)             const;
    juce::Rectangle<int> modeBtnBounds (int block)             const;
    juce::Rectangle<int> slotBounds    (int block, int slot)   const;
};
