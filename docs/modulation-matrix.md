# Modulation Matrix

## Overview

The modulation matrix routes any **source** signal to any **destination** parameter with a scalable amount. It has 32 independent slots processed every audio block.

```
dest_value += source_value × amount
```

Multiple slots can share the same destination — their contributions accumulate.

## Sources (25 + None)

| ID | Source | Range | Notes |
|---|---|---|---|
| 0 | None | — | Slot inactive |
| 1–4 | LFO1–LFO4 | −1 to +1 | After shape + amount scaling |
| 5 | Env_Master | 0 to +1 | Master ADSR output |
| 6 | Env_3D | 0 to +1 | 3D oscillator ADSR |
| 7 | Env_FM | 0 to +1 | FM envelope |
| 8 | Env_WT | 0 to +1 | Wavetable envelope |
| 9 | Env_VA | 0 to +1 | Maximum of the eight virtual analog layer envelopes |
| 10 | Env_FilterA | 0 to +1 | Filter A envelope |
| 11 | Env_FilterB | 0 to +1 | Filter B envelope |
| 12 | Velocity | 0 to +1 | MIDI note velocity |
| 13 | Aftertouch | 0 to +1 | Polyphonic key pressure for the exact channel/note voice |
| 14 | ModWheel | 0 to +1 | MIDI CC1 |
| 15 | PitchBend | −1 to +1 | MIDI pitch bend (bipolar) |
| 16 | CC74 | 0 to +1 | MIDI CC74; MPE master broadcasts and member channels remain per-note |
| 17 | ChanPressure | 0 to +1 | Channel pressure with MPE master/member routing |
| 18–25 | Macro1–Macro8 | 0 to +1 | Panel macro knobs |

## Destinations (252 + None)

| ID | Destination | Base range | Modulation effect |
|---|---|---|---|
| 0 | None | — | — |
| 1 | OscPitch_All | ±12 semitones | Pitch in semitones |
| 2 | Osc3D_Radius | 0.1–4.0 | Added to base radius |
| 3 | Osc3D_Depth | 0–4.0 | Added to base depth |
| 4 | Osc3D_Warp | 0–2.0 | Added to warp amount |
| 5 | Osc3D_Chaos | 0–1.0 | Added, clamped |
| 6 | Osc3D_Morph | 0–1.0 | Added, clamped |
| 7 | Osc3D_Level | 0–1.0 | Added, clamped |
| 8–17 | FM_Op0–9_Level | 0–1.0 | Added to FM A operator level |
| 18–27 | FM_Op0–9_Ratio | — | Additive FM A operator ratio offset |
| 28 | WT_Position | 0–1.0 | Wavetable scan position |
| 29 | WT_Level | 0–1.0 | Wavetable output level |
| 30 | VA_PW | 0.05–0.95 | Pulse width |
| 31 | VA_Detune | 0–1.0 | Unison detune |
| 32 | VA_Spread | 0–1.0 | Stereo spread |
| 33 | VA_Level | 0–1.0 | VA output level |
| 34–36 | FilterA_Cutoff/Res/Drive | — | Added to base values |
| 37–39 | FilterB_Cutoff/Res/Drive | — | Added to base values |
| 40–43 | LFO1–4_Rate | Hz | Added to LFO rate |
| 44–47 | LFO1–4_Amount | 0–1.0 | LFO amount scale |
| 48 | Master_Volume | 0–1.0 | Master gain |
| 49 | Master_Pan | −1 to +1 | Stereo pan |
| 50 | Arp_Gate | 0–1.0 | Gate length |
| 51 | Arp_Rate | — | Arp step rate |
| 52 | Osc3D_FieldFreq | 0.25–16.0 | Adds to field-frequency multiplier |
| 53 | Osc3D_ZRatio | 0.25–4.0 | Adds to z-axis motion ratio |
| 54 | Osc3D_Fold | 0–1.0 | Adds to coordinate fold amount |
| 55 | Osc3D_Drive | 0.25–4.0 | Adds to output shaper drive |
| 56 | VA_Shape | 0–5.0 | Adds to VA 1 shape morph, scaled across the full shape range |
| 57 | VA_Ring | 0–1.0 | Adds to VA 1 ring modulation amount |
| 58 | VA_Fold | 0–1.0 | Adds to VA 1 wavefold amount |
| 59–65 | VA2_Shape/PW/Detune/Spread/Level/Ring/Fold | — | Same controls for VA layer 2 |
| 66–72 | VA3_Shape/PW/Detune/Spread/Level/Ring/Fold | — | Same controls for VA layer 3 |
| 73–79 | VA4_Shape/PW/Detune/Spread/Level/Ring/Fold | — | Same controls for VA layer 4 |
| 80–83 | LFO1–4_SyncRate | 0–10 | Adds to synced LFO rate index, scaled across the sync-rate table |
| 84–85 | WT_Warp / WT_PD_Break | 0–1.0 | Wavetable warp amount and phase-distortion breakpoint |
| 86–92 | Gran_Position/Size/Density/Pitch/Spray/Freeze/Level | — | Granular cloud position, timing, pitch, spread, freeze, and output level |
| 93–96 | Gran_BodyMix/Size/Damping/Inharm | — | Optional granular physical body blend and material controls |
| 97–101 | EngFilt3D_Cutoff/Res/Drive/Morph/Mix | — | Per-voice 3D oscillator engine filter controls before Filter A/B |
| 102–106 | EngFiltFM_Cutoff/Res/Drive/Morph/Mix | — | Per-voice FM engine filter controls before Filter A/B |
| 107–111 | EngFiltWT_Cutoff/Res/Drive/Morph/Mix | — | Per-voice wavetable engine filter controls before Filter A/B |
| 112–116 | EngFiltVA_Cutoff/Res/Drive/Morph/Mix | — | Per-voice virtual analog engine filter controls before Filter A/B |
| 117–121 | EngFiltKS_Cutoff/Res/Drive/Morph/Mix | — | Per-voice Karplus-Strong engine filter controls before Filter A/B |
| 122–126 | EngFiltGran_Cutoff/Res/Drive/Morph/Mix | — | Per-voice granular engine filter controls before Filter A/B |
| 127–131 | EngFiltTerrain_Cutoff/Res/Drive/Morph/Mix | — | Per-voice terrain engine filter controls before Filter A/B |
| 132–138 | VA5_Shape/PW/Detune/Spread/Level/Ring/Fold | — | Same controls for VA layer 5 |
| 139–145 | VA6_Shape/PW/Detune/Spread/Level/Ring/Fold | — | Same controls for VA layer 6 |
| 146–152 | VA7_Shape/PW/Detune/Spread/Level/Ring/Fold | — | Same controls for VA layer 7 |
| 153–159 | VA8_Shape/PW/Detune/Spread/Level/Ring/Fold | — | Same controls for VA layer 8 |
| 160–168 | Terrain_CenterX/CenterY/RadiusX/RadiusY/RatioX/RatioY/Smoothing/Morph/Drive | — | Direct wave-terrain scan-path and shaping offsets for terrain slot 1 |
| 169–178 | FM_B_Op0–9_Level | 0–1.0 | Added to FM B operator level (independent from FM A) |
| 179–188 | FM_B_Op0–9_Ratio | — | Additive FM B operator ratio offset (independent from FM A) |
| 189–197 | Terrain2_CenterX/CenterY/RadiusX/RadiusY/RatioX/RatioY/Smoothing/Morph/Drive | — | Same controls for terrain slot 2 |
| 198–206 | Terrain3_* (same 9 suffixes) | — | Same controls for terrain slot 3 |
| 207–215 | Terrain4_* (same 9 suffixes) | — | Same controls for terrain slot 4 |
| 216–224 | Terrain5_* (same 9 suffixes) | — | Same controls for terrain slot 5 |
| 225–233 | Terrain6_* (same 9 suffixes) | — | Same controls for terrain slot 6 |
| 234–242 | Terrain7_* (same 9 suffixes) | — | Same controls for terrain slot 7 |
| 243–251 | Terrain8_* (same 9 suffixes) | — | Same controls for terrain slot 8 |

Terrain oscillator slots have independent envelopes and APVTS parameters. The terrain engine filter destinations above modulate the terrain pre-filter. Each terrain slot (Terrain_* for slot 1, Terrain2_* through Terrain8_* for slots 2-8) has its own independent center X/Y, radius X/Y, ratio X/Y, smoothing, morph, and drive destinations, matching the VA2-VA8 per-instance convention.

FM A (`FM_Op*_Level`/`FM_Op*_Ratio`) and FM B (`FM_B_Op*_Level`/`FM_B_Op*_Ratio`) have fully independent operator modulation destinations; a matrix slot targeting one engine no longer affects the other.

The VA5-VA8, direct terrain, FM_B, and Terrain2-8 destinations are appended after earlier destinations in enum declaration order to preserve older preset destination IDs.

## Bipolar vs Unipolar

Each slot has a **bipolar** toggle:

| Mode | Source 0.8 → internal value |
|---|---|
| Bipolar (default) | `0.8 × 2 - 1 = 0.6` (center at 0) |
| Unipolar | `0.8` (center at 0.5) |

Use **unipolar** for sources like velocity, envelopes, or macros where 0 means "none" and 1 means "full." LFOs and pitch bend are already naturally bipolar, so the matrix leaves those source values unchanged even when the bipolar toggle is enabled.

## Processing Order

```
1. LFO params are applied and `LFOEngine::processBlock()` produces one value per LFO for the block
2. Mod slots and macro parameters are read from cached APVTS pointers
3. `buildModSources()` copies LFO, current envelope, MIDI, and macro values into `ModValues`
4. `ModMatrix::process()` accumulates destination offsets
5. `syncParams()` applies destination offsets to engine params
6. `EnvelopeBank::processAndGet()` advances envelopes per sample during voice rendering
```

## Slot Parameters

Each slot N (0–31) has four APVTS parameters:

```
mod{N}_source   — int, ModSource enum index
mod{N}_dest     — int, ModDest enum index
mod{N}_amount   — float -1..1
mod{N}_bipolar  — bool
```

## Mod Step Sequencer

The mod step sequencer is a four-lane performance modulator that writes directly into modulation destinations once per audio block. Each lane has a destination and 32 stored step values.

Timing and length parameters:

```
modseq_depth        — float 0..1, output depth
modseq_division     — choice, base note division
modseq_subdivision  — choice, x1/x2/x4/x8 step multiplier
modseq_length       — choice, stored pattern length: 8/16/24/32
modseq_loop_length  — int 1..32, active playback loop length
```

`modseq_loop_length` is a performance trim. Playback wraps at `min(modseq_length, modseq_loop_length)`, so you can remove steps down to a one-step pulse without erasing or moving the programmed pattern.

Step-sequencer lanes use bipolar output for terrain center X/Y, radius X/Y, and ratio X/Y so terrain scan motion can move around the base path instead of only adding positive offsets. Terrain smoothing, morph, and drive use unipolar step output.

In the editor, steps beyond the active loop length are dimmed but remain editable. This makes it possible to shorten a phrase during performance, edit the parked steps, and bring the full stored pattern back without reprogramming it.

## Design Patterns

### Velocity → Filter cutoff (velocity sensitivity)
```
Slot 0: source=Velocity, dest=FilterA_Cutoff, amount=+0.5, bipolar=OFF
```
Higher velocity → higher cutoff. `amount=+0.5` means max velocity adds 2 octaves of cutoff (0.5 × 4 octave range).

### LFO → Pitch (vibrato)
```
Slot 1: source=LFO1, dest=OscPitch_All, amount=+0.05, bipolar=ON
```
LFO1 set to Sine, 5 Hz, depth 1.0. Amount 0.05 → ±0.6 semitones.

### Envelope → Wavetable position (timbral evolution)
```
Slot 2: source=Env_Master, dest=WT_Position, amount=+0.8, bipolar=OFF
```
Wavetable scans from position 0 at attack to position 0.8 at sustain.

### Aftertouch → FM operator level (expression)
```
Slot 3: source=Aftertouch, dest=FM_Op3_Level, amount=+1.0, bipolar=OFF
```
Pressing harder brings in FM modulator Op3.

### Macro → Multiple destinations (performance macro)
```
Slot 4: source=Macro1, dest=Osc3D_Chaos, amount=+1.0, bipolar=OFF
Slot 5: source=Macro1, dest=FilterA_Resonance, amount=+0.6, bipolar=OFF
```
One macro knob sweeps both chaos and resonance simultaneously.

### LFO → 3D field frequency
```
Slot 6: source=LFO2, dest=Osc3D_FieldFreq, amount=+0.5, bipolar=ON
```
LFO2 gently shifts harmonic density without changing played pitch.

### Aftertouch → 3D fold and drive
```
Slot 7: source=Aftertouch, dest=Osc3D_Fold,  amount=+0.4, bipolar=OFF
Slot 8: source=Aftertouch, dest=Osc3D_Drive, amount=+0.8, bipolar=OFF
```
Pressing harder pushes the scan path into more mirrored, saturated field regions.

## Adding a New Destination

1. Add entry to `ModDest` enum in [ModDest.h](../Source/Engine/Modulation/ModDest.h)
2. Apply the destination value in `SynthVoice::syncParams()`:
   ```cpp
   float myParam = baseValue + modDests.dests[(int)ModDest::MyNewDest];
   ```
3. Update the `mod{N}_dest` parameter choice list in `PluginProcessor.cpp`
4. Update the `kDestNames` array in `ModMatrixPanel.cpp`
