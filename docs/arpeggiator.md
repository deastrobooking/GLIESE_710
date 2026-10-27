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
| As Played | Steps through notes in the order they were pressed or latched |
| Random | Picks a random note each step (no repeat guarantee) |
| Random Latch | Shuffles the active notes once, then repeats that order until the active note list changes |

## Timing

Step length is controlled by `arp_rate` (index into a fixed rate table) and `arp_sync`.

| Index | Quarter-note beats | Musical value |
|---|---|---|
| 0 | 1/8 | Thirty-second note |
| 1 | 1/4 | Sixteenth note |
| 2 | 1/2 | Eighth note |
| 3 | 3/4 | Dotted eighth (default) |
| 4 | 1 | Quarter note |
| 5 | 1.5 | Dotted quarter |
| 6 | 2 | Half note |
| 7 | 3 | Dotted half |
| 8 | 4 | Whole note |

When `arp_sync = true`, tempo is taken from the DAW's play-head BPM (forwarded by `GLIESE710AudioProcessor::processBlock`). When false, the arpeggiator uses an internal 120 BPM clock.

## Swing

`arp_swing` (0.0–0.66) alternates long and short step lengths around the base rate:

```
longStepSamples  = stepSamples × (1 + swing × 0.5)
shortStepSamples = stepSamples × (1 - swing × 0.5)
```

At `swing = 0`: straight. Higher values push the alternating long/short feel harder. The cap at 0.66 keeps step lengths practical.

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

When `arp_latch = true`, note-ons toggle notes in the latch buffer. Press a note once to add it, press it again to remove it. Releasing all keys does **not** stop the arpeggiator — it keeps cycling the latched notes until they are toggled out or an all-notes-off/reset message arrives.

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
| `Arp_Gate` | Adds to gate fraction, averaged across active voices and clamped to 0.01–1.0 |
| `Arp_Rate` | Adds a rounded offset to the rate index, averaged across active voices and clamped to the rate table |

Use `LFO → Arp_Gate` for rhythmic accents, or `Macro → Arp_Gate` for a performance-friendly gate sweep.

## Implementation Notes

- The arpeggiator maintains fixed-capacity note lists: `heldNotes` (currently pressed keys in played order), `latchedNotes` (toggled latch notes in played order), plus cached sorted/random-latch order buffers. This keeps MIDI handling allocation-free on the audio thread.
- Timing is tracked in fractional samples (`stepPhase`) to avoid drift across blocks.
- When host sync is off, the arpeggiator runs at 120 BPM. When sync is on and the host has not supplied a new BPM, it continues at the last known BPM.
- `RandomLatch` fixes a shuffled order until the active note list changes, giving a consistent but non-sequential pattern.

## Compatibility notes

The rate table uses quarter-note beats, matching host BPM. Earlier builds interpreted whole-note fractions as beats and played four times too fast. Parameter IDs and choice ordering are unchanged; existing patches now follow the displayed musical rate.

Changing `arp_on` releases sounding voices and clears held/latched arpeggiator notes. Press new keys after switching modes. This prevents stuck notes and stale latch playback.
