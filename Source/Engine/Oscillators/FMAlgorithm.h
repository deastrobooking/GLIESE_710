#pragma once
#include "FMOperator.h"
#include <array>

// Routing descriptor for one algorithm
// opModulators[i] is a bitmask of which operators modulate operator i
// carriers is a bitmask of operators whose output goes to the final mix
struct AlgorithmDef
{
    uint16_t opModulators[10]; // bit j set → op j modulates op i
    uint16_t carriers;         // bit i set → op i is a carrier (output)
};

class FMAlgorithm
{
public:
    static constexpr int kNumOperators  = 10;
    static constexpr int kNumAlgorithms = 40; // 32 DX7-style + 8 extended

    void prepare(double sampleRate);
    void setAlgorithm(int index);
    void setNoteFrequency(float hz);

    FMOperator& getOperator(int i) { return ops[i]; }

    void noteOn();
    void noteOff();
    void reset();

    // Process one sample and return carrier sum (normalised)
    float processSample();

    static const AlgorithmDef& getAlgorithmDef(int index);

private:
    std::array<FMOperator, kNumOperators> ops;
    int algorithmIndex = 0;

    static AlgorithmDef buildAlgorithms();
    static const AlgorithmDef kAlgorithms[kNumAlgorithms];
};
