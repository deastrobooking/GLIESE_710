# GLIESE 710

**A hybrid cinematic synthesizer for 3D function, FM, wavetable, granular, terrain, VA, and physical-string synthesis.**

GLIESE 710 is a polyphonic C++20 / JUCE 8 synthesizer plugin. Its original center is the **3D Function Oscillator**, where a scan path moves through a mathematical field instead of reading a normal waveform. The current engine expands that idea into a two-page, mix-and-match oscillator system with independent wavetable, granular, wave-terrain, virtual analog, Karplus-Strong, and dual FM engines.

The design goal is direct: every oscillator page should feel playable and modular without the controls leaking across pages. Page 1 and Page 2 can each host a full FM engine, or each page can host four oscillator slots for up to eight independent non-FM oscillators.

---

## Formats

| Format | Platform |
|---|---|
| AU | macOS |
| VST3 | macOS / Windows |
| LV2 | Linux |
| Standalone | All platforms |

---

## Quick Start

### Prerequisites

| Tool | Minimum version |
|---|---|
| CMake | 3.22 |
| C++ compiler | C++20 (Clang 13, GCC 11, MSVC 2022) |
| Xcode | 14+ (macOS AU builds only) |
| Git | Any recent version |

JUCE 8 is fetched automatically by CMake via CPM.

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

### Production Build Folder

```bash
cmake -S . -B build-productions -DCMAKE_BUILD_TYPE=Release -DGLIESE_BUILD_TESTS=ON -DGLIESE_COPY_PLUGIN_AFTER_BUILD=OFF
cmake --build build-productions --config Release --parallel
```

Set `GLIESE_COPY_PLUGIN_AFTER_BUILD=ON` only when you explicitly want JUCE to copy built plugins into system plugin folders.

### Run Tests

```bash
cmake -B build -DGLIESE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Feature Overview

### Oscillators

| Engine | Description |
|---|---|
| **3D Function** | Eight independent slot instances, 18 scalar fields, five path types, domain warping, coordinate folding, field-frequency control, chaos injection, field morphing, and drive. |
| **FM A / FM B** | Two fully separate 10-operator FM engines with independent algorithms, operator controls, ADSRs, detune, phase position, algorithm morph, and modulation-index scaling. |
| **Wavetable** | Eight independent wavetable slots with FFT mipmap bandlimiting, Catmull-Rom interpolation, table morphing, 15 factory tables, 15 warp modes, CZ-style phase distortion, unison, stereo spread, position spread, phase, random phase, and retrigger controls. |
| **Granular** | Eight independent preallocated grain engines with 96 grains per instance, free-cloud/burst/audio-rate modes, position, size, density, pitch, spray, jitter, pan spread, reverse probability, scan/freeze, seven window shapes, and optional physical body/modal resonator. |
| **Wave Terrain** | Eight independent 2D terrain oscillators. A phase-driven path scans a procedural or imported heightmap with circle, ellipse, Lissajous, spiral, line, and orbit-noise modes. |
| **Virtual Analog** | Eight independent VA layers with continuous shape morph, PolyBLEP saw/square/pulse, reverse saw, triangle, sine, 8-voice unison, stereo spread, ring modulation, and wave folding. |
| **Karplus-Strong** | Eight independent plucked-string slots with circular delay loop, LPF brightness, all-pass stiffness, excitation morphing, pluck position, and per-slot ADSR. |

Each oscillator slot has its own enable state, level, octave transpose, and ADSR envelope. Duplicate oscillator types now use separate parameter prefixes, so changing one wavetable, granular, terrain, VA, or KS slot does not change the others.

### Engine Pages

The oscillator tab is divided into two blocks:

| Page mode | Behavior |
|---|---|
| **FM** | The whole page becomes one FM engine view. Page 1 controls FM A. Page 2 controls FM B. |
| **Oscillators** | The page shows four oscillator slots. Each slot can be 3D, wavetable, granular, wave terrain, VA, or Karplus-Strong. |

This gives two intended layouts:

- **Dual FM layout**: FM A on Page 1 and FM B on Page 2.
- **Eight oscillator layout**: four mix-and-match oscillator slots per page.

The slot bar supports these type IDs: `None`, `3D`, `VA`, `WT`, `KS`, `Granular`, and `Terrain`.

### FM Performance Controls

Each FM engine exposes large performance controls for:

```
Master Detune
Phase Position
Algorithm Morph
Mod Index Scale
```

These sit above the operator grid in the FM pages. They are designed for real-time movement: detuned cinematic drift, phase-position changes between attacks, crossblended algorithms, and single-knob harmonic intensity swells.

### Wave-Terrain Oscillator

The terrain oscillator reads a 2D height surface instead of a 1D waveform:

```text
phase -> scan path x/y -> heightmap lookup -> output sample
```

Black maps to `-1.0`, middle gray to `0.0`, and white to `+1.0`. If no image is loaded, the oscillator uses a procedural default terrain.

Important controls:

```text
Path Mode
Center X/Y
Radius X/Y
Path Ratio X/Y
Phase Offset
Spiral
Drive
Offset
Smoothing
Morph
Gain
```

See [docs/terrain-oscillator.md](docs/terrain-oscillator.md).

### Dual Filter

Two filter slots can run in series, parallel, A only, or B only. Each slot has smoothed cutoff, resonance, drive, morph, saturation, bass compensation, gain, mix, envelope depth, LFO depth, and optional Off/2x/4x quality for nonlinear models.

Filter modes:

```text
LP12  LP24  HP12  HP24
Bandpass  Notch  Peak  LowShelf  HighShelf
Comb  AM+  AM-
Ladder12  Ladder18  Ladder24
SEM Morph  MS Dual  Formant
Juno-106  Juno-60  Jupiter-4  Jupiter-8
TB-303
P5 Rev2 SSM  P5 Rev3 CEM  Prophet-600  Prophet-6  Pro-One
```

See [docs/filter.md](docs/filter.md).

### Modulation Matrix

32 slots. Each slot: **Source -> Destination -> Amount (-1 to +1) -> Bipolar toggle**.

Sources include LFO1-4, envelope groups, velocity, aftertouch, mod wheel, pitch bend, CC74, channel pressure, and eight macros. Destinations cover 3D controls, FM operator level/ratio controls, wavetable position/warp, granular controls, VA controls, filter controls, LFO controls, master volume/pan, and arp rate/gate.

Terrain modulation destinations are not exposed yet; terrain currently responds through its direct APVTS parameters and per-slot envelope.

### Arpeggiator

Seven patterns: Up, Down, Up-Down, Down-Up, As Played, Random, Random Latch. Host-sync rate, gate, swing, latch, and 1-4 octave range are available.

### Preset System

Presets save as human-readable XML in `~/Music/GLIESE_710/Presets/` with the `.g710` extension. DAW session recall uses JUCE state serialization through `getStateInformation()` and `setStateInformation()`.

---

## Repository Layout

```text
GLIESE_710/
├── CMakeLists.txt
├── cmake/CPM.cmake
├── docs/
│   ├── architecture.md
│   ├── oscillator-3d.md
│   ├── terrain-oscillator.md
│   ├── fm-engine.md
│   ├── granular-oscillator.md
│   ├── karplus-strong.md
│   ├── modulation-matrix.md
│   ├── filter.md
│   ├── arpeggiator.md
│   ├── preset-system.md
│   ├── build-targets.md
│   └── contributing.md
├── Source/
│   ├── PluginProcessor.{h,cpp}
│   ├── PluginEditor.{h,cpp}
│   ├── Engine/
│   │   ├── Oscillators/   # Osc3D, FM, WT, Granular, Terrain, VA, KS
│   │   ├── Envelopes/
│   │   ├── LFO/
│   │   ├── Filter/
│   │   ├── Modulation/
│   │   └── Arpeggiator/
│   ├── Presets/
│   └── UI/
├── Assets/Wavetables/
└── Tests/EngineTests.cpp
```

---

## Roadmap

- [x] Independent two-page oscillator layout
- [x] Independent FM A / FM B engines
- [x] Runtime wavetable import path + FFT-based mipmap generation
- [x] Granular physical body / modal resonator section
- [x] Roland, TB-303, and Sequential-style filter character modes
- [x] Wave-terrain oscillator MVP
- [ ] Terrain image chooser and waveform/path visualizer in the editor
- [ ] Terrain mipmaps / anti-aliasing quality modes
- [ ] FM algorithm graph visualization
- [ ] 3D field visualizer panel
- [ ] MPE support
- [ ] GitHub Actions CI
- [ ] Factory preset bank

---

## License

Personal/indie use under the [JUCE Personal License](https://juce.com/juce-personal-license-restrictions/). For commercial distribution, a JUCE commercial license is required.
