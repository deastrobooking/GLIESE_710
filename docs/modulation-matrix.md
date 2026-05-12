# Modulation Matrix

## Overview

The modulation matrix routes any **source** signal to any **destination** parameter with a scalable amount. It has 32 independent slots processed every audio block.

```
dest_value += source_value × amount
```

Multiple slots can share the same destination — their contributions accumulate.

## Sources (25)

| ID | Source | Range | Notes |
|---|---|---|---|
| 0 | None | — | Slot inactive |
| 1–4 | LFO1–LFO4 | −1 to +1 | After shape + amount scaling |
| 5 | Env_Master | 0 to +1 | Master ADSR output |
| 6 | Env_3D | 0 to +1 | 3D oscillator ADSR |
| 7 | Env_FM | 0 to +1 | FM envelope |
| 8 | Env_WT | 0 to +1 | Wavetable envelope |
| 9 | Env_VA | 0 to +1 | Virtual analog envelope |
| 10 | Env_FilterA | 0 to +1 | Filter A envelope |
| 11 | Env_FilterB | 0 to +1 | Filter B envelope |
| 12 | Velocity | 0 to +1 | MIDI note velocity |
| 13 | Aftertouch | 0 to +1 | MIDI aftertouch (channel pressure) |
| 14 | ModWheel | 0 to +1 | MIDI CC1 |
| 15 | PitchBend | −1 to +1 | MIDI pitch bend (bipolar) |
| 16 | CC74 | 0 to +1 | MIDI CC74 (expression) |
| 17 | ChanPressure | 0 to +1 | MIDI channel pressure |
| 18–25 | Macro1–Macro8 | 0 to +1 | Panel macro knobs |

## Destinations (50)

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
| 8–17 | FM_Op0–9_Level | 0–1.0 | Added to op level |
| 18–22 | FM_Op0–4_Ratio | — | Additive ratio offset |
| 23 | WT_Position | 0–1.0 | Wavetable scan position |
| 24 | WT_Level | 0–1.0 | Wavetable output level |
| 25 | VA_PW | 0.05–0.95 | Pulse width |
| 26 | VA_Detune | 0–1.0 | Unison detune |
| 27 | VA_Spread | 0–1.0 | Stereo spread |
| 28 | VA_Level | 0–1.0 | VA output level |
| 29–31 | FilterA_Cutoff/Res/Drive | — | Added to base values |
| 32–34 | FilterB_Cutoff/Res/Drive | — | Added to base values |
| 35–38 | LFO1–4_Rate | Hz | Added to LFO rate |
| 39–42 | LFO1–4_Amount | 0–1.0 | LFO amount scale |
| 43 | Master_Volume | 0–1.0 | Master gain |
| 44 | Master_Pan | −1 to +1 | Stereo pan |
| 45 | Arp_Gate | 0–1.0 | Gate length |
| 46 | Arp_Rate | — | Arp step rate |
| 47 | Osc3D_FieldFreq | 0.25–16.0 | Adds to field-frequency multiplier |
| 48 | Osc3D_ZRatio | 0.25–4.0 | Adds to z-axis motion ratio |
| 49 | Osc3D_Fold | 0–1.0 | Adds to coordinate fold amount |
| 50 | Osc3D_Drive | 0.25–4.0 | Adds to output shaper drive |

## Bipolar vs Unipolar

Each slot has a **bipolar** toggle:

| Mode | Source 0.8 → internal value |
|---|---|
| Bipolar (default) | `0.8 × 2 - 1 = 0.6` (center at 0) |
| Unipolar | `0.8` (center at 0.5) |

Use **unipolar** for sources like velocity, envelopes, or macros where 0 means "none" and 1 means "full." LFOs and pitch bend are already naturally bipolar, so the matrix leaves those source values unchanged even when the bipolar toggle is enabled.

## Processing Order

```
1. LFO::processBlock(numSamples)   ← one value per LFO per block
2. EnvelopeBank::processAndGet()   ← per sample in voice render loop
3. buildModSources()               ← copy LFO+env values into ModValues
4. ModMatrix::process()            ← accumulate destinations
5. syncParams()                    ← apply dest offsets to engine params
```

## Slot Parameters

Each slot N (0–31) has four APVTS parameters:

```
mod{N}_source   — int, ModSource enum index
mod{N}_dest     — int, ModDest enum index
mod{N}_amount   — float -1..1
mod{N}_bipolar  — bool
```

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
