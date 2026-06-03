# AGENTS

This file is project memory for coding agents working in GLIESE 710.

## Build Folders

- Use `build-productions/` as the canonical local CMake build directory.
- `build/` and `build-production/` are legacy/local folders and may contain stale plugin bundles.
- Build directories are generated artifacts and must not be committed.
- If generated build files appear in `git status`, clean them from the index or restore/remove them before finishing.

## Build Commands

Configure the normal production build:

```bash
scripts/cmake.zsh --preset production
```

Build only the VST3 bundle:

```bash
scripts/cmake.zsh --build --preset vst3 --parallel
```

Build all plugin formats:

```bash
scripts/cmake.zsh --build --preset plugin --parallel
```

Build and run tests:

```bash
scripts/cmake.zsh --build --preset tests --parallel
scripts/ctest.zsh --preset tests
```

The VST3 bundle to copy into a DAW is:

```text
build-productions/GLIESE_710_artefacts/Release/VST3/GLIESE 710.vst3
```

If an editor change does not appear in the VST3, force a clean wrapper rebuild:

```bash
cmake --build build-productions --target GLIESE_710_VST3 --config Release --clean-first
```

Important: `GLIESE_710` is the shared code target. It does not refresh the VST3 wrapper bundle by itself.

The wrapper scripts prefer system CMake/CTest when available and fall back to
the local portable CMake bundle, which is useful on machines where plain
`cmake` is not on `PATH`.

## Current Engine Notes

- The editor has an **ENGINE FILTERS** tab.
- Each oscillator family has a per-voice stereo filter before the master dual filter: 3D, FM, WT, VA, KS, GRAN, and TERRAIN.
- Per-engine filters use `engine_filter_{3d,fm,wt,va,ks,gran,terrain}_*` APVTS parameter prefixes.
- Per-engine filter modulation destinations are named `EngFilt{3D,FM,WT,VA,KS,Gran,Terrain}_{Cutoff,Resonance,Drive,Morph,Mix}`.
- The master filter mode list includes `Alt Ladder` and `Phys Ladder`.
- Filter model changes should run `GLIESE_Tests "[filter]"`; that suite includes a multi-frequency resonance signature check for every `FilterMode`, plus focused character checks for Alt Ladder, Phys Ladder, TB-303, Roland, Sequential, and GLIESE Prism modes.
- Audio-thread or oscillator performance changes should run `GLIESE_Tests "[performance]"` in addition to the full CTest suite.
- Every oscillator family has both a fine tune parameter (`*_tune`, cents) and an integer octave parameter (`*_octave`).
- Arpeggiator note storage is fixed-capacity for audio-thread safety; avoid reintroducing dynamic note-list mutation in render-path MIDI handling.
- Terrain smoothing/mip data is precomputed when terrain data is generated or imported; do not rebuild terrain vectors from render-path parameter updates.
