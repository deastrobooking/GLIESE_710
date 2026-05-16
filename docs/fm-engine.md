# FM Engine

## Overview

GLIESE 710 has two independent **10-operator FM engines**:

```text
Page 1 FM mode -> FM A
Page 2 FM mode -> FM B
```

Each engine has its own APVTS parameters, algorithm, level, octave, ADSR, master detune, phase position, algorithm morph, modulation-index scale, and ten independent operators. Turning on the second FM page does not reuse the first engine's controls.

## Engine Prefixes

| Engine | Engine prefix | Operator prefix |
|---|---|---|
| FM A | `osc_fm_` | `op0_` through `op9_` |
| FM B | `osc_fm2_` | `op2_0_` through `op2_9_` |

Example:

```text
osc_fm_algorithm       FM A algorithm
osc_fm2_algorithm      FM B algorithm
op3_level              FM A operator 3 level
op2_3_level            FM B operator 3 level
```

## Operator Model

Each `FMOperator` is an oscillator with a selectable waveform:

```text
frequency = noteHz * ratio + fine/detune offsets
phase     = phase + frequency / sampleRate
output    = wave(phase + phaseOffset + modInput + feedback * prevOutput)
output   *= level * velocityScale * keyScale * operatorEnvelope
```

Operator controls:

| Control | Purpose |
|---|---|
| `coarse` | Ratio selection. `0` maps to 0.5x, `1` maps to 1x, then integer ratios upward. |
| `fine` | Continuous fine ratio/pitch trim in semitones. |
| `detune` | Small cents offset for beating and movement. |
| `level` | Operator amplitude. Carriers become audible; modulators set FM index. |
| `feedback` | Self-feedback depth for grit, noise, or wind-like digital textures. |
| `wave` | Sine, triangle, saw, square, pulse, noise, and FM-X-inspired spectral forms. |
| `skirt` | Harmonic rolloff for spectral forms. |
| `resonance` | Resonant harmonic emphasis for resonant spectral forms. |
| `key_reset` | Resets phase on note-on for stable attacks. |
| `vel` | Velocity-to-level depth. |
| `key` | Keyboard-level scaling around C4. |
| `adsr_*` | Per-operator amplitude envelope. |

## Performance Controls

Each FM engine has four higher-level controls intended for real-time sound design:

| Parameter | Range | Description |
|---|---|---|
| `osc_fm*_detune` | -100 to +100 cents | Master engine detune. Useful for FM A/B chorusing and slow beating. |
| `osc_fm*_phase` | 0-1 | Master phase position offset. Changes the attack relationship between operators. |
| `osc_fm*_algo_morph` | 0-1 | Crossblends the selected algorithm toward the next algorithm. |
| `osc_fm*_index` | 0-2 | Scales operator modulation contribution for pure-to-complex timbral swells. |

`osc_fm_*` refers to FM A. `osc_fm2_*` refers to FM B.

## Algorithm Encoding

Algorithms are directed graphs of operators:

```cpp
struct AlgorithmDef
{
    uint16_t opModulators[10]; // which operators modulate each target op
    uint16_t carriers;         // which operators route to output
};
```

Examples:

```text
Algorithm 0:  Op9 -> Op8 -> ... -> Op1 -> Op0 -> output
Algorithm 5:  Op0..Op9 all carriers, additive style
Algorithm 9:  Op0 <-> Op1 cross-FM pair, both carriers
```

Algorithms 0-31 preserve DX-style topology intent. Algorithms 32-39 add extended GLIESE topologies such as all-to-all, star, ring, and tree routing.

## Algorithm Morph

`algo_morph` blends the current algorithm with the next algorithm in the table. This is intentionally a performance feature rather than strict historical FM behavior.

Use cases:

```text
0.00 = selected algorithm
0.50 = halfway between selected and next topology
1.00 = next topology
```

At low operator levels, morphing behaves like a subtle routing color. At high index or feedback, it can become dramatic and chaotic, which is useful for cinematic textures and risers.

## Processing Order

FM algorithms can include feedback loops, so the engine processes operators once per sample using previous-sample state where needed:

```text
opOut = lastOutputs

for op = 9 down to 0:
    modInput = sum(opOut[modulator] * pi * indexScale)
    opOut[op] = operators[op].processSample(modInput)

lastOutputs = opOut
output = average(carrier outputs)
```

This keeps simple downward chains immediate while cross-FM and feedback paths remain stable.

## Parameters

### Engine-Level

| Parameter | Range | Description |
|---|---|---|
| `osc_fm_on`, `osc_fm2_on` | bool | Enable FM A / FM B. |
| `osc_fm_algorithm`, `osc_fm2_algorithm` | 0-39 | Algorithm selection. |
| `osc_fm_level`, `osc_fm2_level` | 0-1 | Engine output level. |
| `osc_fm_octave`, `osc_fm2_octave` | -3 to +3 | Octave transpose. |
| `osc_fm_detune`, `osc_fm2_detune` | -100 to +100 cents | Master detune. |
| `osc_fm_phase`, `osc_fm2_phase` | 0-1 | Phase position. |
| `osc_fm_algo_morph`, `osc_fm2_algo_morph` | 0-1 | Algorithm crossblend. |
| `osc_fm_index`, `osc_fm2_index` | 0-2 | Modulation index scale. |
| `osc_fm_adsr_*`, `osc_fm2_adsr_*` | envelope | Engine-level amplitude envelope. |

### Operators

For FM A, use `op{N}_*`. For FM B, use `op2_{N}_*`.

| Suffix | Range | Description |
|---|---|---|
| `coarse` | 0-31 | Frequency ratio. |
| `fine` | -1 to +1 | Fine offset in semitones. |
| `detune` | -15 to +15 cents | Operator detune. |
| `level` | 0-1 | Operator amplitude. |
| `feedback` | 0-1 | Self-feedback. |
| `fixed` | bool | Fixed-frequency mode path. |
| `wave` | 0-11 | Sine, basic shapes, noise, and spectral forms. |
| `skirt` | 0-1 | Spectral rolloff. |
| `resonance` | 0-1 | Spectral resonance position. |
| `key_reset` | bool | Reset phase at note-on. |
| `vel` | 0-1 | Velocity sensitivity. |
| `key` | -1 to +1 | Keyboard level scaling. |
| `adsr_a/d/s/r` | envelope | Per-operator envelope. |

## Modulation Matrix Notes

The current FM modulation destinations target operator levels and the first five operator ratios. These destinations are engine-shared by design right now: a matrix target such as `FM_Op3_Level` applies to the corresponding operator path when FM A or FM B is synced.

Future work can split these into `FM_A_*` and `FM_B_*` destinations if we need fully separate modulation routings per FM page.

## Adding A Custom Algorithm

In [FMAlgorithm.cpp](../Source/Engine/Oscillators/FMAlgorithm.cpp), extend the algorithm table:

```cpp
{
    { 0b0000001010,   // op0 modulated by op1 and op3
      0b0000000100,   // op1 modulated by op2
      0, 0, 0, 0, 0, 0, 0, 0 },
    0b0000000001      // op0 is carrier
},
```

Then update `kNumAlgorithms` and the algorithm parameter ranges if the total count changes.

## Source Files

- [FMAlgorithm.h](../Source/Engine/Oscillators/FMAlgorithm.h)
- [FMAlgorithm.cpp](../Source/Engine/Oscillators/FMAlgorithm.cpp)
- [FMOperator.h](../Source/Engine/Oscillators/FMOperator.h)
- [FMOperator.cpp](../Source/Engine/Oscillators/FMOperator.cpp)
