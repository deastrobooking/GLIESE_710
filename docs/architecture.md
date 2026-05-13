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
    │       ├── [LFO Engine]  (4 LFOs — processed once per block)
    │       ├── [Mod Matrix]  (32 slots: source → dest accumulation)
    │       │
    │       ├── Osc3D          → env3D  ─╮  field freq / z ratio / warp / fold / drive
    │       ├── FMAlgorithm    → envFM  ─┤  additive mix
    │       ├── WavetableOsc   → envWT  ─┤
    │       ├── VAOscillator ×4 → envVA1-4 ─╯
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
| `SynthVoice` | `Osc3D`, `FMAlgorithm`, `WavetableOsc`, `VAOscillator[4]`, `EnvelopeBank`, `LFOEngine`, `DualFilter`, `ModMatrix` | Engine/ |
| `FMAlgorithm` | `FMOperator[10]` | Engine/Oscillators/ |
| `DualFilter` | `SingleFilter × 2`, `ADSREnvelope × 2` | Engine/Filter/ |
| `EnvelopeBank` | `ADSREnvelope[10]` | Engine/Envelopes/ |

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
      → set frequency on all oscillators
      → envBank.noteOn()  ← triggers all 10 voice envelopes
      → fm.noteOn()       ← triggers per-operator envelopes
      → dualFilter.noteOn()

noteOff(note)
  → voice.noteOff()
      → envBank.noteOff()  ← starts release on all envelopes
      → fm.noteOff()
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
│  │  Tab 1 — OSCILLATORS                             │   │
│  │    3D OscPanel │ VA tabbed panel │ WT OscPanel   │   │
│  │    VA tabbed panel contains VA 1–4               │   │
│  │                                                  │   │
│  │  Tab 2 — FM ENGINE                               │   │
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

**OscPanel** is a generic panel parameterized by `oscId` (`"3d"`, `"va"`, `"va2"`, `"va3"`, `"va4"`, `"wt"`). It shows a title label, enable toggle, level/octave knobs, per-oscillator ADSR, mode combo boxes, and type-specific extra sliders — all wired to APVTS parameter IDs prefixed with `osc_{oscId}_`.

To change the tab layout, edit `MainPanel::resized()` in [Source/UI/MainPanel.cpp](../Source/UI/MainPanel.cpp).
