#include "FMAlgorithm.h"

// ─── Algorithm table ──────────────────────────────────────────────────────────
// Encoding: opModulators[i] = bitmask of operators that feed INTO op i
//           carriers = bitmask of operators whose outputs go to audio out
//
// Op indices 0..9.  We define 40 algorithms.
// DX7 has 6 ops (32 algs); here we extend to 10 ops.
// Algs 0..31 map to DX7 concepts scaled to 10 ops.
// Algs 32..39 are custom extended topologies.

const AlgorithmDef FMAlgorithm::kAlgorithms[FMAlgorithm::kNumAlgorithms] = {
    // Alg 0: Single carrier, 9 modulators in series
    // Op0 modulated by Op1, Op1 by Op2, ..., Op8 by Op9; Op0 is carrier
    {
        { 0b0000000010, 0b0000000100, 0b0000001000, 0b0000010000,
          0b0000100000, 0b0001000000, 0b0010000000, 0b0100000000,
          0b1000000000, 0 },
        0b0000000001
    },
    // Alg 1: Two carriers, two stacks of 5
    // Stack A: Op0←Op1←Op2←Op3←Op4; Stack B: Op5←Op6←Op7←Op8←Op9
    {
        { 0b0000000010, 0b0000000100, 0b0000001000, 0b0000010000, 0,
          0b0001000000, 0b0010000000, 0b0100000000, 0b1000000000, 0 },
        0b0000100001
    },
    // Alg 2: 3 carriers (Op0,Op3,Op6) each with 3 modulators
    {
        { 0b0000000010, 0b0000000100, 0, 0b0000010000, 0b0000100000, 0,
          0b0010000000, 0b0100000000, 0b1000000000, 0 },
        0b0001001001
    },
    // Alg 3: 5 parallel carrier pairs (Op0+Op1, Op2+Op3, Op4+Op5, Op6+Op7, Op8+Op9)
    {
        { 0b0000000010, 0, 0b0000001000, 0, 0b0000100000, 0,
          0b0010000000, 0, 0b1000000000, 0 },
        0b0101010101
    },
    // Alg 4: Op0 modulated by all others
    {
        { 0b1111111110, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        0b0000000001
    },
    // Alg 5: 10 parallel carriers (additive)
    {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        0b1111111111
    },
    // Alg 6: Op0 carrier; Op1 carrier; Op2-9 form a modulator chain into Op1
    {
        { 0, 0b0000000100, 0b0000001000, 0b0000010000, 0b0000100000,
          0b0001000000, 0b0010000000, 0b0100000000, 0b1000000000, 0 },
        0b0000000011
    },
    // Alg 7: Three carriers, one shared modulator ring
    {
        { 0b0000000010, 0, 0b0000000010, 0, 0b0000000010, 0, 0, 0, 0, 0 },
        0b0010101001
    },
    // Alg 8: Feedback network — Op0 self-feedback, feeds Op1 and Op2, both carriers
    {
        { 0b0000000001, 0b0000000001, 0b0000000001, 0, 0, 0, 0, 0, 0, 0 },
        0b0000000110
    },
    // Alg 9: X-mod — Op0 mods Op1, Op1 mods Op0 (cross-FM), both carriers
    {
        { 0b0000000010, 0b0000000001, 0, 0, 0, 0, 0, 0, 0, 0 },
        0b0000000011
    },
    // Algs 10-31: additional DX7-inspired topologies (abbreviated here)
    // These follow increasing numbers of carriers / decreasing mod depth
    { { 0b0000001110, 0, 0, 0, 0b0001110000, 0, 0, 0, 0, 0 }, 0b0010000001 },
    { { 0b0000000010, 0b0000001100, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b1111110001 },
    { { 0, 0b0000000010, 0b0000000100, 0, 0, 0, 0, 0, 0, 0 }, 0b1111111001 },
    { { 0b0000000110, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b1111111001 },
    { { 0b0000001010, 0, 0b0000000010, 0, 0, 0, 0, 0, 0, 0 }, 0b1111110001 },
    { { 0, 0, 0, 0, 0, 0b0001000000, 0, 0, 0, 0 }, 0b1111011111 },
    { { 0b0000000010, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b1111111101 },
    { { 0b0000000100, 0b0000000100, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b1111111001 },
    { { 0b0000001000, 0b0000001000, 0b0000001000, 0, 0, 0, 0, 0, 0, 0 }, 0b1111110111 },
    { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b0000000011 },
    { { 0b0000000010, 0b0000000100, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b1111111001 },
    { { 0, 0b0000001000, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b1111111011 },
    { { 0b0000001110, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b1111110001 },
    { { 0b0010101010, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b0000000001 },
    { { 0b0000000010, 0, 0b0000000010, 0, 0b0000000010, 0, 0, 0, 0, 0 }, 0b0000010101 },
    { { 0b0000001110, 0, 0, 0, 0, 0b0001100000, 0, 0, 0, 0 }, 0b0000110001 },
    { { 0, 0, 0, 0b0001110000, 0, 0, 0, 0, 0, 0 }, 0b1111000001 },
    { { 0b0001111110, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b0000000001 },
    { { 0b0000000010, 0b0000001100, 0, 0, 0b0000010000, 0, 0, 0, 0, 0 }, 0b1111100001 },
    { { 0b0000000010, 0b0000000100, 0b0000001000, 0b0000010000, 0, 0, 0, 0, 0, 0 }, 0b1111100001 },
    { { 0, 0b0000001110, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b1111110001 },
    { { 0b0000001110, 0, 0, 0, 0b0001100000, 0, 0, 0, 0, 0 }, 0b0000110001 },
    // Algs 32-39: Extended custom topologies
    // Alg 32: Cascade of 5 pairs
    { { 0b0000000010, 0b0000001100, 0, 0b0001110000, 0, 0, 0, 0, 0, 0 }, 0b0001001001 },
    // Alg 33: All-to-all modulation (chaos)
    { { 0b1111111110, 0b1111111101, 0b1111111011, 0b1111110111, 0b1111101111,
        0b1111011111, 0b1110111111, 0b1101111111, 0b1011111111, 0b0111111111 },
      0b0000000001 },
    // Alg 34: Star topology, Op9 modulates all
    { { 0b1000000000, 0b1000000000, 0b1000000000, 0b1000000000, 0b1000000000,
        0b1000000000, 0b1000000000, 0b1000000000, 0b1000000000, 0 },
      0b0111111111 },
    // Alg 35: Ring of modulators feeding single carrier
    { { 0b0000000010, 0b0000000100, 0b0000001000, 0b0000010000, 0b0000100000,
        0b0001000000, 0b0010000000, 0b0100000000, 0b0000000001, 0 },
      0b0000000001 },
    // Alg 36: Two cross-FM pairs + 6 carriers
    { { 0b0000000010, 0b0000000001, 0b0000001000, 0b0000000100, 0, 0, 0, 0, 0, 0 },
      0b1111110011 },
    // Alg 37: Triple stacks of 3 + single carrier
    { { 0b0000000110, 0b0000000100, 0, 0b0000110000, 0b0000100000, 0, 0b1100000000, 0b1000000000, 0, 0 },
      0b0010010001 },
    // Alg 38: Op0 is mega-carrier modulated by tree of 9
    { { 0b0000001110, 0b0000010000, 0b0000100000, 0b0001000000, 0, 0b0010000000, 0, 0b0100000000, 0, 0b1000000000 },
      0b0000000001 },
    // Alg 39: Pure additive (10 independent carriers, no modulation)
    { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0b1111111111 },
};

//==============================================================================
const AlgorithmDef& FMAlgorithm::getAlgorithmDef(int index)
{
    return kAlgorithms[juce::jlimit(0, kNumAlgorithms - 1, index)];
}

void FMAlgorithm::prepare(double sampleRate)
{
    for (auto& op : ops) op.prepare(sampleRate);
}

void FMAlgorithm::setAlgorithm(int index)
{
    algorithmIndex = juce::jlimit(0, kNumAlgorithms - 1, index);
}

void FMAlgorithm::setNoteFrequency(float hz)
{
    for (auto& op : ops) op.setNoteFrequency(hz);
}

void FMAlgorithm::setVelocity(float v)
{
    for (auto& op : ops) op.setVelocity(v);
}

void FMAlgorithm::setMidiNote(int note)
{
    for (auto& op : ops) op.setMidiNote(note);
}

void FMAlgorithm::noteOn()
{
    lastOutputs.fill(0.f);
    for (auto& op : ops) op.noteOn();
}

void FMAlgorithm::noteOff()
{
    for (auto& op : ops) op.noteOff();
}

void FMAlgorithm::reset()
{
    for (auto& op : ops) op.reset();
    lastOutputs.fill(0.f);
}

float FMAlgorithm::processSample()
{
    const auto& alg = kAlgorithms[algorithmIndex];
    auto opOut = lastOutputs;

    // Process high-index modulators first. Feedback/cross-FM paths use the
    // previous sample's output, avoiding double-advancing operator phases.
    for (int i = kNumOperators - 1; i >= 0; --i)
    {
        float modInput = 0.f;
        uint16_t mods = alg.opModulators[i];
        for (int j = 0; j < kNumOperators; ++j)
        {
            if (mods & (1 << j))
                modInput += opOut[(size_t) j] * juce::MathConstants<float>::pi;
        }

        opOut[(size_t) i] = ops[(size_t) i].processSample(modInput);
    }

    lastOutputs = opOut;

    // Sum carriers
    float sum = 0.f;
    int   numCarriers = 0;
    for (int i = 0; i < kNumOperators; ++i)
    {
        if (alg.carriers & (1 << i))
        {
            sum += opOut[(size_t) i];
            ++numCarriers;
        }
    }
    return (numCarriers > 0) ? sum / (float)numCarriers : 0.f;
}
