# Architecture Overview

## Signal Flow

```
MIDI Input
    │
    ▼
Sample-accurate MIDI / Arpeggiator scheduler
    │
    ▼
SynthEngine  (voice pool — 10 voices, oldest-note stealing)
    │
    ├── SynthVoice × 10
    │       │
    │       ├── [LFO Engine]    (4 LFOs — processed once per block)
    │       ├── [Mod Matrix]    (32 slots: source → dest accumulation)
    │       │
    │       ├── Osc3D ×6        → env3D    ─╮  ensemble detune / field / path / warp / fold / drive
    │       ├── FMAlgorithm ×2  → envFM    ─┤  dual-engine detuned chorus (2nd engine optional)
    │       ├── KarplusStrong   → envKS    ─┤  plucked on noteOn, self-decaying
    │       ├── WavetableOsc    → envWT    ─┤  additive mix
    │       ├── GranularOsc ×8  → envGRAN  ─┤  preallocated stereo grain cloud
    │       ├── VAOscillator ×8 → envVA1-4 ─╯
    │       │
    │       ├── DualFilter (Filter A → Filter B  or  parallel)
    │       │       └── per-filter ADSR envelopes
    │       │
    │       └── Master ADSR × volume × pan
    │
    ▼
Stereo Output Buffer (accumulated across all active voices)
```

## Module Ownership

| Class | Owns | Lives in |
|---|---|---|
| `GLIESE710AudioProcessor` | `SynthEngine`, `PresetManager`, `APVTS` | PluginProcessor |
| `SynthEngine` | `SynthVoice[10]`, `Arpeggiator`, master age/feedback effects | Engine/ |
| `SynthVoice` | `Osc3D[8]`, `FMAlgorithm×2`, `KarplusStrong[4]`, `WavetableOsc[4]`, `GranularOsc[8]`, `VAOscillator[8]`, `EnvelopeBank`, `LFOEngine`, `DualFilter`, `ModMatrix` | Engine/ |
| `FMAlgorithm` | `FMOperator[10]` | Engine/Oscillators/ |
| `GranularOsc` | fixed sample buffer, `Grain[96]`, trigger clock, deterministic RNG | Engine/Oscillators/ |
| `KarplusStrong` | circular delay buffer, LPF state, APF state | Engine/Oscillators/ |
| `DualFilter` | `SingleFilter × 2`, Clean SVF/Ladder/SEM/MS/Formant/Comb model cores, `ADSREnvelope × 2` | Engine/Filter/ |
| `EnvelopeBank` | `ADSREnvelope[12]` (master, 3D, FM, WT, VA×4, KS, granular, filterA, filterB) | Engine/Envelopes/ |

## Thread Model

```
Message Thread          Audio Thread
─────────────           ─────────────
APVTS writes            APVTS reads (atomic)
Preset load/save        processBlock()
UI repaints             No allocation
                        No locks
                        No system calls
```

**Rule:** nothing in `processBlock()` or any method called from it may allocate memory, lock a mutex, or call a system API. The arpeggiator reserves its note vectors during prepare, and MIDI rendering uses member buffers rather than per-block heap allocation.

## Parameter System

All controls are registered as `juce::AudioProcessorParameter` children of `AudioProcessorValueTreeState` (APVTS). This gives:

- Thread-safe reads from the audio thread via `getRawParameterValue()` → `std::atomic<float>*`
- Automatic DAW automation
- Free undo/redo
- One-call preset serialization via `copyState()` → `ValueTree` → XML

Parameter pointers are cached as `std::atomic<float>*` during engine setup with a render-time fallback for unusual host lifecycle ordering. Values are read once per active voice block in `SynthVoice::renderBlock`, then applied to sub-engines through `SynthVoice::syncParams()`. See [docs/modulation-matrix.md](modulation-matrix.md) for how mod matrix offsets are layered on top of base parameter values.

## MIDI Timing

`SynthEngine::renderNextBlock()` renders voice audio in segments split at MIDI event sample positions. This keeps note-on/note-off timing sample-accurate within the host block.

When the arpeggiator is enabled, incoming note events update the arpeggiator at their original sample positions. Generated arp events are merged into a member MIDI buffer, then the same segmented renderer plays the resulting events. See [docs/arpeggiator.md](arpeggiator.md) for full parameter and pattern documentation.

Host BPM is read by `GLIESE710AudioProcessor::processBlock()` and passed into the synth engine. The engine forwards it to the arpeggiator and each voice's LFO engine for sync-rate calculations.

## Voice Lifecycle

```
noteOn(note, vel)
  → findFreeVoice() or stealVoice() (oldest age)
      → voice.noteOn(note, vel)
      → set frequency on all Osc3D[6], fm, fm2, wtOsc, vaOscs[4], ksOsc
      → ksOsc.pluck(velocity, cachedKSParams)  ← KS triggered immediately
      → envBank.noteOn()  ← triggers all 12 voice envelopes
      → fm.noteOn(), fm2.noteOn()  ← per-operator envelopes
      → dualFilter.noteOn()

noteOff(note)
  → voice.noteOff()
      → envBank.noteOff()  ← starts release on all envelopes
      → fm.noteOff(), fm2.noteOff()
      → dualFilter.noteOff()

Voice::isActive()
  → true while envBank.anyActive()
  → returns false when master ADSR reaches Idle
  → voice is then available for reuse
```

Every envelope in `EnvelopeBank` is advanced each rendered sample, even if the corresponding oscillator level is zero. This prevents silent oscillators or filter envelopes from holding a voice active forever.

## Adding a New Module

1. Create `Source/Engine/YourModule/YourModule.{h,cpp}`
2. Add `void prepare(double sampleRate)` and `void reset()`
3. Add it as a member of `SynthVoice`
4. Call `prepare()` in `SynthVoice::prepare()`
5. Call `reset()` in `SynthVoice::reset()`
6. Register any new parameters in `PluginProcessor.cpp` → `buildLayout()`
7. Cache/read params and call your module in `SynthVoice::buildParamCache()` and `SynthVoice::syncParams()`
8. Add the `.cpp` to `target_sources` in `CMakeLists.txt`

## UI Layout

The editor is a `GLIESE710AudioProcessorEditor` (default 1200×780, resizable 900–1920 × 600–1200). It has two direct children, stacked vertically:

```
┌──────────────────────────────────────────────────────────┐
│  PresetBrowser  (36 px, top strip)                       │
├──────────────────────────────────────────────────────────┤
│  MainPanel  (remaining height)                           │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │  TabbedComponent  (TabsAtTop, depth 28 px)       │   │
│  │                                                  │   │
│  │  Tab 1 — OSCILLATORS                                      │   │
│  │    Slot bar + Page 1 / Page 2 selector                    │   │
│  │    Each page is either an FM engine view or 4 oscillator slots │ │
│  │    WT panel: table, warp mode, position, unison, phase controls ││
│  │                                                  │   │
│  │  Tab 2 — FM CONTROLS                             │   │
│  │    Enable + algorithm + FM level/octave + ADSR   │   │
│  │    Rows: Op0–Op9 coarse, level, wave, velocity,  │   │
│  │    and key-scale controls                        │   │
│  │                                                  │   │
│  │  Tab 3 — FILTER / MOD / ARP                      │   │
│  │    FilterPanel  (top 240 px)                     │   │
│  │    Bottom row: ModMatrixPanel | LFOPanel | ArpPanel│  │
│  └──────────────────────────────────────────────────┘   │
│                                                          │
│  Master strip  (bottom 124 px)                           │
│    MASTER │ Vol Tune Oct Pan Port Age Feed PB MW │ A D S R│
└──────────────────────────────────────────────────────────┘
```

**Color scheme**: industrial dark faceplate, background `#050607`, and per-module accent colors from `IndustrialTheme`.

**OscPanel** is a generic panel parameterized by `oscId` (`"3d"`, `"ks"`, `"va"`, `"va2"`, `"va3"`, `"va4"`, `"wt"`). It shows a title label, enable toggle, level/octave knobs, per-oscillator ADSR, mode combo boxes, and type-specific extra sliders — all wired to APVTS parameter IDs prefixed with `osc_{oscId}_`.

To change the tab layout, edit `MainPanel::resized()` in [Source/UI/MainPanel.cpp](../Source/UI/MainPanel.cpp).
