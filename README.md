# GLIESE 710

**A 3D Function Oscillator Synthesizer Plugin**

GLIESE 710 is a polyphonic synthesizer plugin built in C++20 and JUCE 8. Its centerpiece is a novel **3D Function Oscillator** — instead of selecting a waveform, you design the mathematical space the waveform travels through. Around it sits a 10-operator FM engine (optionally dual-engine), anti-aliased wavetable oscillator, four virtual analog oscillator layers, Karplus-Strong string synthesis, dual multimode filter, 32-slot modulation matrix, 4 LFOs, arpeggiator, and a full preset system.

---

## Formats

| Format     | Platform         |
|------------|------------------|
| AU         | macOS            |
| VST3       | macOS / Windows  |
| LV2        | Linux            |
| Standalone | All platforms    |

---

## Quick Start

### Prerequisites

| Tool        | Minimum version |
|-------------|-----------------|
| CMake       | 3.22            |
| C++ compiler| C++20 (Clang 13, GCC 11, MSVC 2022) |
| Xcode       | 14+ (macOS AU builds only)           |
| Git         | Any recent version                   |

JUCE 8 is fetched automatically by CMake via CPM — no manual download required.

### Build (macOS / Linux)

```bash
git clone <repo-url>
cd GLIESE_710

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

### Build (Windows)

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Run Standalone (fastest iteration)

```bash
# macOS / Linux
./build/GLIESE_710_artefacts/Release/Standalone/GLIESE\ 710.app/Contents/MacOS/GLIESE\ 710

# or open the .app bundle directly on macOS
open build/GLIESE_710_artefacts/Release/Standalone/GLIESE\ 710.app
```

### Run Tests

```bash
cmake -B build -DGLIESE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## Feature Overview

### Oscillators

| Engine                | Description |
|-----------------------|-------------|
| **3D Function**       | 8 musical scalar fields, 5 path types, up to 6-voice ensemble with per-voice detune, domain warping, coordinate folding, field-frequency control, chaos injection, field morphing |
| **FM (10-op)**        | 10 operators, 40 algorithms (DX7-compatible 0–31 + 8 extended), optional dual-engine mode with independent detune, per-operator ADSR + feedback |
| **Karplus-Strong**    | Plucked string synthesis — circular delay loop, LPF brightness, all-pass stiffness, excitation morphing (white noise → glitch burst), pluck position |
| **Wavetable**         | 2048-sample tables, 10 mipmap levels, Catmull-Rom interpolation, table morphing |
| **Virtual Analog**    | 4 independent VA layers, continuous shape morph, PolyBLEP saw/square/pulse, reverse saw, triangle, sine, 8-voice unison, stereo spread, ring modulation, wave folding |

All engines mix additively. Each engine or layer has an independent level, octave transpose, and ADSR envelope.

### Engine Configuration

The oscillators tab exposes a configuration strip for the 3D and FM engines:

| Control       | Range | Effect |
|---------------|-------|--------|
| **3D Voices** | 1–6   | How many Osc3D instances run in parallel per voice — loudness-normalized (÷√N) |
| **3D Detune** | 0–1 st| Semitone spread across the ensemble; instances spread symmetrically around center pitch |
| **Dual FM**   | on/off| Activates a second FM engine sharing the same algorithm and operator params |
| **FM Detune** | 0–100 ¢| Cent offset for the second FM engine — creates a detuned chorus effect |

### 3D Oscillator Fields

```
Harmonic   sin(x·F) + a·sin(2y·F+z) + b·sin(3z·F+x) — harmonic series layering
Chord      x=root, y=third (major→minor via morphT), z=fifth — chordal axis ratios
Shell      sin(dist·k·F) — concentric spherical resonance shells
Gravity    gravity wells at root / fifth / octave positions in 3D space
Formant    Gaussian envelope functions along each axis — vowel-like formant shaping
Metallic   blend integer ratios → √2 / φ via chaos param — harmonic vs irrational
Tension    stable harmonic blend + tense nonlinear product, morphed by morphT
Body       x=body y=edge z=air — timbral axis model (warpAmt/chaos control blend)
```

### 3D Path Types

```
Circle        r·cos(t), r·sin(t), d·sin(t/2)
Spiral        expanding/contracting orbit
Lissajous     3:2:5 ratio Lissajous figure
RandomWalk    damped Brownian motion (chaos-scaled)
PhysicsOrbit  elliptical orbit with eccentricity
```

Key 3D oscillator controls:

```
Field Freq   harmonic-density multiplier inside the scalar field
Z Ratio      z-axis path-rate multiplier for stable/evolving motion
Warp         bends coordinates before field evaluation
Fold         mirrors coordinates back into the central field region
Drive        soft-saturation amount after field evaluation
Morph        crossfades to the next field
```

### Dual Filter

12 modes per filter slot, two slots in series or parallel:

```
LP12  LP24  HP12  HP24
Bandpass  Notch
Peak  LowShelf  HighShelf
Comb (feedback comb / metallic)
AM+  AM- (ring modulation variants)
```

### Modulation Matrix

32 slots. Each slot: **Source → Destination → Amount (−1 to +1) → Bipolar toggle**

**25 sources** including LFO1–4, 7 envelope source groups, velocity, aftertouch, mod wheel, pitch bend, CC74, channel pressure, and 8 macro knobs.

**78 usable destinations** including 3D oscillator math controls, FM operator levels and ratios, wavetable position, all VA layer shape/PW/detune/spread/level/ring/fold controls, both filter cutoffs/resonances/drives, LFO rates/amounts/sync rates, master volume/pan, arp gate/rate.

### Arpeggiator

7 patterns: Up, Down, Up-Down, Down-Up, As Played, Random, Random Latch  
Host-sync (1/32 to 1 bar), gate (0–100%), swing (0–66%), latch mode, 1–4 octave range.

### Preset System

Presets save as human-readable XML in `~/Music/GLIESE_710/Presets/` with the `.g710` extension. DAW session recall uses the standard `getStateInformation`/`setStateInformation` path — no extra steps needed.

---

## Repository Layout

```
GLIESE_710/
├── CMakeLists.txt
├── cmake/CPM.cmake
├── docs/                        ← Developer documentation
│   ├── architecture.md
│   ├── oscillator-3d.md
│   ├── 3D_OSC_math.md
│   ├── fm-engine.md
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
│   │   ├── Oscillators/         ← Osc3D, FMAlgorithm, WavetableOsc, VAOscillator, KarplusStrong
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

- [ ] 2×/4× oversampling quality mode for nonlinear 3D oscillator patches (`juce::dsp::Oversampling`)
- [ ] FM algorithm graph visualization
- [ ] 3D field visualizer panel (OpenGL)
- [ ] KS per-voice stereo width (pair of delay lines with independent detune)
- [ ] User wavetable import (drag-and-drop `.wav`)
- [ ] FFT-based mipmap generation (WavetableOsc)
- [ ] MPE support
- [ ] GitHub Actions CI (macOS AU+VST3, Windows VST3, Linux LV2)
- [ ] Factory preset bank

---

## License

Personal/indie use under the [JUCE Personal License](https://juce.com/juce-personal-license-restrictions/). For commercial distribution a JUCE commercial license is required.
