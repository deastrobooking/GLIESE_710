# Arpeggiator

## Overview

The arpeggiator converts held or latched notes into a rhythmic step sequence. It runs in `SynthEngine` alongside the voice pool and generates sample-accurate note-on/off events that feed into the same voice scheduler as live MIDI input.

## Signal Path

```
Incoming MIDI
    │
    ├─ (arp_on = true)
    │       │
    │       ▼
    │   Arpeggiator::handleMidi()   ← updates held/latched note list
    │       │
    │       ▼
    │   Arpeggiator::processBlock() ← emits note-on/off to arpMidi buffer
    │       │
    │       ▼
    │   SynthEngine (renders arpMidi as if it were live MIDI)
    │
    └─ (arp_on = false) → MIDI passes through unchanged
```

## Patterns

| Pattern | Behaviour |
|---|---|
| Up | Notes sorted ascending, step through lowest→highest |
| Down | Notes sorted descending |
| Up-Down | Ascending then descending (shared endpoints) |
| Down-Up | Descending then ascending |
| As Played | Steps through notes in the order they were pressed |
| Random | Picks a random note each step (no repeat guarantee) |
| Random Latch | Like Random but remembers a fixed random order until notes change |

## Timing

Step length is controlled by `arp_rate` (index into a fixed rate table) and `arp_sync`.

| Index | Beat fraction | Musical value |
|---|---|---|
| 0 | 1/32 | Thirty-second note |
| 1 | 1/16 | Sixteenth note |
| 2 | 1/8 | Eighth note |
| 3 | 3/16 | Dotted eighth (default) |
| 4 | 1/4 | Quarter note |
| 5 | 3/8 | Dotted quarter |
| 6 | 1/2 | Half note |
| 7 | 3/4 | Dotted half |
| 8 | 1 bar | Whole note |

When `arp_sync = true`, tempo is taken from the DAW's play-head BPM (forwarded by `GLIESE710AudioProcessor::processBlock`). When false, a fixed internal tempo is used.

## Swing

`arp_swing` (0.0–0.66) delays every other step by a fraction of the step length:

```
swingAmount = swing × stepSamples
odd steps: start on time
even steps: start + swingAmount late
```

At `swing = 0`: straight. At `swing = 0.5`: triplet-feel swing. The cap at 0.66 prevents steps from overlapping.

## Octaves

`arp_octaves` (1–4) transposes the pattern up by one octave after each complete cycle, then resets:

```
Cycle 1: pattern at original pitch
Cycle 2: pattern + 12 semitones
Cycle 3: pattern + 24 semitones (if octaves ≥ 3)
...then back to cycle 1
```

## Gate

`arp_gate` (0.01–1.0) controls the note duration as a fraction of the step length:

- `0.01`: near-staccato
- `0.5`: 50% gate (typical punchy rhythm)
- `1.0`: legato — note held until the next step fires

## Latch Mode

When `arp_latch = true`, held notes are copied into a latch buffer when a new note-on arrives. Releasing all keys does **not** stop the arpeggiator — it keeps cycling the latched notes. To reset latch, press any new key (which replaces the latch buffer).

## Parameters

| Parameter | Type | Range | Default | Description |
|---|---|---|---|---|
| `arp_on` | bool | — | false | Enable/disable arpeggiator |
| `arp_pattern` | choice | 0–6 | 0 (Up) | Step pattern |
| `arp_rate` | choice | 0–8 | 3 (dotted 8th) | Step length |
| `arp_sync` | bool | — | true | Sync to DAW BPM |
| `arp_octaves` | int | 1–4 | 1 | Octave range |
| `arp_gate` | float | 0.01–1.0 | 0.8 | Note gate fraction |
| `arp_swing` | float | 0.0–0.66 | 0.0 | Swing amount |
| `arp_latch` | bool | — | false | Latch note list |

## Modulation Matrix Destinations

| Destination | Effect |
|---|---|
| `Arp_Gate` | Adds to gate fraction (clamped 0–1) |
| `Arp_Rate` | Adds to rate index |

Use `LFO → Arp_Gate` for rhythmic accents, or `Macro → Arp_Gate` for a performance-friendly gate sweep.

## Implementation Notes

- The arpeggiator maintains two note lists: `heldNotes` (currently pressed keys) and `latchedNotes`. `activeNotes()` returns whichever is relevant given the latch state.
- Timing is tracked in fractional samples (`stepPhase`) to avoid drift across blocks.
- When the host is not playing (no BPM available), the arpeggiator continues running at the last known BPM.
- RandomLatch fixes a shuffled order until the active note list changes, giving a consistent but non-sequential pattern.
