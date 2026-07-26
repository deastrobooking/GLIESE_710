# GLIESE 710

[![CI](https://github.com/deastrobooking/GLIESE_710/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/deastrobooking/GLIESE_710/actions/workflows/ci.yml)
[![Release](https://github.com/deastrobooking/GLIESE_710/actions/workflows/release.yml/badge.svg)](https://github.com/deastrobooking/GLIESE_710/actions/workflows/release.yml)

**A hybrid cinematic synthesizer for 3D function, FM, wavetable, granular, terrain, VA, and physical-string synthesis.**

GLIESE 710 is a polyphonic C++20 / JUCE 8 synthesizer plugin. Its original center is the **3D Function Oscillator**, where a scan path moves through a mathematical field instead of reading a normal waveform. The current engine expands that idea into a two-page, mix-and-match oscillator system with independent wavetable, granular, wave-terrain, virtual analog, Karplus-Strong, and dual FM engines.

The design goal is direct: every oscillator page should feel playable and modular without the controls leaking across pages. Page 1 and Page 2 can each host a full FM engine, or each page can host four oscillator slots for up to eight independent non-FM oscillators.

---

## Formats

| Format | Platform |
|---|---|
| AUv2 / AUv3 | macOS |
| VST3 | macOS / Windows |
| CLAP | macOS / Windows / Linux |
| LV2 | Linux |
| Standalone | All platforms |

Host/plugin scanner vendor metadata is `ECOSYSTEM`.

---

## Quick Start

### Prerequisites

| Tool | Minimum version |
|---|---|
| CMake | 3.22 |
| C++ compiler | C++20 (Clang 13, GCC 11, MSVC 2022) |
| Xcode | 14+ (macOS AU and signed release builds) |
| Git | Any recent version |

JUCE 8 is fetched automatically by CMake via CPM from the pinned release archive. When CLAP support is enabled, the CLAP wrapper, CLAP core, and CLAP helper sources are also fetched from pinned archives.

### Build

```bash
scripts/cmake.zsh --preset production
scripts/cmake.zsh --build --preset vst3 --parallel
```

The canonical local build folder is `build-productions/`. Older local folders named
`build/` or `build-production/` may contain stale plugin bundles; do not copy VST3s
from them unless you intentionally configured those folders yourself.

### Automated Production Build

For website/distribution release packaging and publishing, see
`docs/deployment.md`.

macOS signed release packages are built from the Xcode project in `build-xcode/`
so the AUv3 app extension can be generated and embedded in the Standalone app.
The classic AUv2 bundle remains `GLIESE 710.component`.

For the normal local loop, use the build script instead of manually copying plugin bundles:

```bash
scripts/build-plugin.zsh
```

To build, test, install AUv2/VST3/CLAP into the macOS system plugin folders, ad-hoc sign them, and verify the installed bundles:

```bash
scripts/build-plugin.zsh --install-system
```

Use `--install-user` to install into `~/Library/Audio/Plug-Ins` instead. See [docs/build-targets.md](docs/build-targets.md).

### Production Build Folder

```bash
cmake -S . -B build-productions -DCMAKE_BUILD_TYPE=Release -DGLIESE_BUILD_TESTS=ON -DGLIESE_COPY_PLUGIN_AFTER_BUILD=OFF
scripts/cmake.zsh --build build-productions --target GLIESE_710_VST3 --config Release --parallel
```

Set `GLIESE_COPY_PLUGIN_AFTER_BUILD=ON` only when you explicitly want build-time plugin copying. JUCE handles AUv2/VST3/LV2 copy steps; the CLAP wrapper uses its own user-scope copy path. The build script is preferred for normal macOS installs because it copies AUv2, VST3, and CLAP consistently and verifies the installed bundles.

The current VST3 bundle path is:

```text
build-productions/GLIESE_710_artefacts/Release/VST3/GLIESE 710.vst3
```

If a newly added editor page does not appear in the DAW, force a clean wrapper rebuild:

```bash
scripts/cmake.zsh --build build-productions --target GLIESE_710_VST3 --config Release --clean-first
```

### Run Tests

```bash
scripts/cmake.zsh --preset production
scripts/cmake.zsh --build --preset tests --parallel
scripts/ctest.zsh --preset tests
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
| **Wave Terrain** | Eight independent 2D terrain oscillators. Terrain slots span two slot units for a heightmap/path viewer, and a phase-driven path scans a procedural or imported heightmap with circle, ellipse, Lissajous, spiral, line, and orbit-noise modes plus mipmapped anti-aliasing quality modes. |
| **Virtual Analog** | Eight independent VA layers with continuous shape morph, PolyBLEP saw/square/pulse, reverse saw, triangle, sine, 8-voice unison, stereo spread, ring modulation, and wave folding. |
| **Karplus-Strong** | Eight independent plucked-string slots with circular delay loop, LPF brightness, all-pass stiffness, excitation morphing, pluck position, and per-slot ADSR. |

Each oscillator slot has its own enable state, level, fine tune in cents, octave transpose, and ADSR envelope. Duplicate oscillator types now use separate parameter prefixes, so changing one wavetable, granular, terrain, VA, or KS slot does not change the others.

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
Terrain occupies two slot units when possible, so a page can hold one terrain oscillator plus two compact oscillators, or two terrain oscillators.

### FM Performance Controls

Each FM engine exposes large performance controls for:

```
Master Detune
Fine Tune
Phase Position
Algorithm Morph
Mod Index Scale
```

These sit above the operator grid in the FM pages. They are designed for real-time movement: fine-tuned layer offsets, detuned cinematic drift, phase-position changes between attacks, crossblended algorithms, and single-knob harmonic intensity swells.

### Wave-Terrain Oscillator

The terrain oscillator reads a 2D height surface instead of a 1D waveform:

```text
phase -> scan path x/y -> heightmap lookup -> output sample
```

Black maps to `-1.0`, middle gray to `0.0`, and white to `+1.0`. If no image is loaded, the oscillator uses a procedural default terrain.

Important controls:

```text
Path Mode
Import Map
Center X/Y
Radius X/Y
Path Ratio X/Y
Scan Rate
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
GLIESE Prism  Alt Ladder  Phys Ladder
```

See [docs/filter.md](docs/filter.md).

### Engine Filters

The **ENGINE FILTERS** page adds one per-voice filter for each oscillator family before the master dual filter: 3D, FM, WT, VA, KS, GRAN, and TERRAIN. Each uses the full filter mode list with cutoff, resonance, drive, morph, saturation, bass compensation, mix, quality, envelope depth, and LFO depth.

### Modulation Matrix

32 slots. Each slot: **Source -> Destination -> Amount (-1 to +1) -> Bipolar toggle**.

Sources include LFO1-4, envelope groups, velocity, aftertouch, mod wheel, pitch bend, CC74, channel pressure, and eight macros. Destinations cover 3D controls, FM operator level/ratio controls, wavetable position/warp, granular controls, VA controls, terrain scan/path controls, master filter controls, per-engine filter controls, LFO controls, master volume/pan, and arp rate/gate.

The four-lane mod step sequencer has a performance loop-length knob that can trim playback from the full programmed pattern down to a single step without changing stored step values.

Terrain pre-filter modulation is available through the engine filter destinations, and direct terrain path modulation covers center, radius, ratio, smoothing, morph, and drive.

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
│   ├── mpe.md
│   ├── preset-system.md
│   ├── build-targets.md
│   ├── deployment.md
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
- [x] Roland, TB-303, Sequential-style, GLIESE, alternate ladder, and physical ladder filter character modes
- [x] Per-voice oscillator engine filters before the master dual filter
- [x] Wave-terrain oscillator MVP
- [x] Terrain image chooser and waveform/path visualizer in the editor
- [x] Terrain mipmaps / anti-aliasing quality modes
- [x] Imported terrain embedding and shared per-slot terrain assets
- [x] Per-slot terrain and independent FM A / FM B modulation destinations
- [x] Master safety limiter, output meter, DSP load, and voice telemetry
- [x] Live FM A / FM B algorithm graph visualization
- [x] Live per-slot 3D field and scan-path visualizer
- [x] Formal lower/upper-zone MPE support with RPN 6 and master/member expression routing
- [x] GitHub Actions CI
- [x] Embedded, read-only factory preset bank
- [x] Adaptive 3D oscillator oversampling for high field-frequency and nonlinear settings
- [x] Granular drag-and-drop sample loading, background decode/resample, immutable voice handoff, and waveform display
- [x] Granular per-slot source regions, equal slices, slice selection, and waveform markers
- [x] Background granular transient detection and automatic transient-based slicing
- [ ] Granular sample dependencies, root-note metadata, named scenes, and live capture

---

## License

Personal/indie use under the [JUCE Personal License](https://juce.com/juce-personal-license-restrictions/). For commercial distribution, a JUCE commercial license is required.
