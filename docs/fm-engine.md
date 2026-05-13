# FM Engine

## Overview

GLIESE 710 implements a **10-operator FM synthesizer** with 40 configurable algorithms. It is compatible with DX7 timbres (algorithms 0–31) and adds 8 extended topologies (algorithms 32–39).

## Operator Model

Each `FMOperator` is an oscillator with a selectable waveform:

```
frequency  = noteFreq × ratio + fine_detune
             or fixedHz (if fixedFreq mode)
output     = sin(2π × phase + modInput + feedback × prevOutput) × level × envelope
phase     += frequency / sampleRate
```

- **Coarse ratio**: integer 0–31 (0 = 0.5×, 1 = 1×, 2 = 2×, ...)
- **Fine**: ±1 semitone continuous offset
- **Ratio offset**: additive modulation amount applied before fine tune, currently exposed for operators 0–4 in the modulation matrix
- **Feedback**: operator feeds its own previous output back into its phase (`feedback × π` radians)
- **Waveform**: sine, triangle, saw, square, pulse, or noise
- **Velocity depth**: scales operator level by note velocity
- **Key scale depth**: raises or lowers operator level by keyboard position relative to C4
- **Fixed freq mode**: operator uses its fixed-frequency path rather than tracking the note. No APVTS knob exposes `fixedHz` yet, so the current UI path uses the operator default.

## Algorithm Encoding

An algorithm is a directed graph of operators. It is encoded as two bitmasks per algorithm:

```cpp
struct AlgorithmDef {
    uint16_t opModulators[10]; // opModulators[i]: which ops modulate op i
    uint16_t carriers;         // which ops route to audio output
};
```

**Example — Algorithm 0** (single carrier, 9 modulators in series):
```
Op9 → Op8 → Op7 → ... → Op1 → Op0 → OUTPUT
```

**Example — Algorithm 5** (pure additive):
```
Op0 Op1 Op2 Op3 Op4 Op5 Op6 Op7 Op8 Op9
 ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓   ↓
                   OUTPUT
```

**Example — Algorithm 9** (cross-FM pair):
```
Op0 ⟷ Op1 → OUTPUT (both are carriers and modulate each other)
```

## Algorithm List

| Range | Description |
|---|---|
| 0 | Single carrier, 9 modulators in series |
| 1 | Two carriers, two stacks of 5 |
| 2 | Three carriers (Op0, Op3, Op6) each with 3 modulators |
| 3 | Five parallel carrier pairs |
| 4 | One carrier modulated by all 9 others |
| 5 | 10 parallel carriers (additive synthesis) |
| 6 | Two carriers, one with 8-op modulator chain |
| 7 | Three carriers sharing one modulator |
| 8 | Self-feedback Op0 feeding two carriers |
| 9 | Cross-FM pair (Op0 ⟷ Op1), both carriers |
| 10–31 | DX7-inspired topologies |
| 32 | Cascade of 5 operator pairs |
| 33 | All-to-all modulation (maximal chaos) |
| 34 | Star topology — Op9 modulates all |
| 35 | Ring of 9 modulators feeding single carrier |
| 36 | Two cross-FM pairs + 6 parallel carriers |
| 37 | Three stacks of 3 + single carrier |
| 38 | Tree of 9 modulators feeding mega-carrier |
| 39 | Pure additive (identical to Alg 5) |

## Processing Order

FM algorithms can contain feedback loops (cross-FM, self-feedback), so a strict topological sort is not always possible. The engine processes each operator once per output sample, walking from high operator indices down to low indices:

```
opOut = lastOutputs

for op = 9 down to 0:
    modInput = sum(opOut[modulator] × π)
    opOut[op] = operators[op].processSample(modInput)

lastOutputs = opOut
output = average(carrier outputs)
```

This lets simple downward chains use same-sample modulation while feedback and cross-FM paths naturally use the previous sample's `lastOutputs`. Each operator phase is advanced exactly once per audio sample, which keeps pitch and envelopes stable.

Operator envelopes are triggered by `FMAlgorithm::noteOn()` and released by `FMAlgorithm::noteOff()` from the voice, so FM notes now follow the same lifecycle as the other oscillator engines.

## Per-Operator Envelopes

Each operator has its own `ADSREnvelope` that scales the operator output level. This allows:

- Modulators that fade out as the note progresses (harmonic evolution)
- Carriers that attack at different rates
- Operators with independent attack, decay, sustain, and release contours under the FM layer envelope

Operator envelope parameters: `op{N}_adsr_{a,d,s,r}` where N is 0–9.

## Parameters

| Parameter | Range | Description |
|---|---|---|
| `osc_fm_on` | bool | Hard enable/disable for the FM engine |
| `osc_fm_algorithm` | 0–39 | Algorithm selection |
| `osc_fm_level` | 0.0–1.0 | Overall FM output level |
| `osc_fm_octave` | −3–+3 | Octave transpose |
| `osc_fm_adsr_{a,d,s,r}` | — | FM engine-level amplitude envelope |
| `op{N}_coarse` | 0–31 | Frequency ratio (0=0.5×, 1=1×, ...) |
| `op{N}_fine` | −1.0–+1.0 | Fine tune in semitones |
| `op{N}_level` | 0.0–1.0 | Operator output level |
| `op{N}_feedback` | 0.0–1.0 | Self-feedback depth |
| `op{N}_fixed` | bool | Fixed frequency mode |
| `op{N}_wave` | 0–5 | Sine / Triangle / Saw / Square / Pulse / Noise |
| `op{N}_vel` | 0.0–1.0 | Velocity depth |
| `op{N}_key` | −1.0–+1.0 | Keyboard level scaling around C4 |
| `op{N}_adsr_{a,d,s,r}` | — | Per-operator envelope |

The modulation matrix can target `FM_Op0_Ratio` through `FM_Op4_Ratio`. These destinations add to the operator ratio before fine tuning, which is useful for controlled ratio bends and evolving inharmonic tones.

## Adding a Custom Algorithm

In [FMAlgorithm.cpp](../Source/Engine/Oscillators/FMAlgorithm.cpp), extend `kAlgorithms[]`:

```cpp
// Example: Op0 is carrier; Op1 modulates Op0; Op2 modulates Op1; Op3 modulates Op0 directly
{
    { 0b0000001010,   // op0 modulated by op1 (bit1) and op3 (bit3)
      0b0000000100,   // op1 modulated by op2
      0, 0, 0, 0, 0, 0, 0, 0 },
    0b0000000001      // op0 is carrier
},
```

Increment `kNumAlgorithms` in [FMAlgorithm.h](../Source/Engine/Oscillators/FMAlgorithm.h) and update the `osc_fm_algorithm` parameter range in `PluginProcessor.cpp`.

## DX7 Compatibility

The FM algorithm and operator concepts are compatible with the Yamaha DX7 (6-operator subset). To emulate a DX7 patch:

- Use operators 0–5 only (set ops 6–9 level to 0)
- Map DX7 algorithm N to algorithm N in GLIESE (algorithms 0–31 preserve DX7 topology intent)
- DX7 EG rates map to ADSR: rate 99 ≈ 0.001s attack, rate 0 ≈ 10s
- DX7 operator level 99 ≈ level 1.0, level 0 = 0.0 (exponential scaling recommended)
