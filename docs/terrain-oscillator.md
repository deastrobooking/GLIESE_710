# Wave-Terrain Oscillator

## Concept

The wave-terrain oscillator turns a 2D surface into an oscillator. Instead of reading a 1D waveform, phase drives a path across a heightmap:

```text
phase -> scan path x/y -> heightmap lookup -> output sample
```

The height at the current path point becomes the audio sample.

```text
black       = -1.0
middle gray =  0.0
white       = +1.0
```

If no image is loaded, `TerrainOsc` generates a procedural default terrain so the oscillator is immediately playable.

## Signal Flow

```text
phase += frequency * scan_rate / sampleRate
path x/y = computePath(phase, params)
height = bilinearSample(terrain, x, y)
height = morph(height, procedural terrain, morph)
output = tanh((height + offset) * drive) * gain
```

Smoothing blends toward a blurred copy of the heightmap. This softens sharp image edges and makes imported terrain less clicky.

## Path Modes

| Mode | Behavior |
|---|---|
| Circle | Stable circular scan. Good default oscillator behavior. |
| Ellipse | Circle with independent X/Y radii. |
| Lissajous | Cross-ratio path using `ratio_x` and `ratio_y`. |
| Spiral | Radial movement through the terrain over the cycle. |
| Line | Straight scan through a region. |
| Orbit Noise | Smooth chaotic orbit around the center. |

## Parameters

Terrain slots use indexed prefixes:

```text
osc_terrain_   # slot 1
osc_terrain2_  # slot 2
...
osc_terrain8_  # slot 8
```

| Suffix | Range | Description |
|---|---|---|
| `on` | bool | Enable slot. |
| `path` | 0-5 | Circle, ellipse, Lissajous, spiral, line, orbit-noise. |
| `center_x` | 0-1 | Horizontal scan center. |
| `center_y` | 0-1 | Vertical scan center. |
| `radius_x` | 0-0.5 | Horizontal path radius. |
| `radius_y` | 0-0.5 | Vertical path radius. |
| `ratio_x` | 0.125-8 | X path ratio. |
| `ratio_y` | 0.125-8 | Y path ratio. |
| `scan_rate` | 0.05-8 | Multiplies the note-following scan speed. |
| `phase` | 0-1 | Path phase offset. |
| `spiral` | 0-1 | Spiral depth / path radial motion. |
| `drive` | 0.1-8 | Pre-output terrain drive. |
| `offset` | -1 to +1 | Height offset before drive. |
| `smoothing` | 0-1 | Blend from raw terrain to blurred terrain. |
| `morph` | 0-1 | Blend toward procedural terrain color. |
| `gain` | 0-2 | Output gain after shaping. |
| `level` | 0-1 | Slot mixer level. |
| `octave` | -3 to +3 | Octave transpose. |
| `adsr_a/d/s/r` | envelope | Per-slot amplitude envelope. |

## Heightmap Import

The Terrain panel has an **IMPORT MAP** button. Click it and choose a bitmap image; the selected heightmap is applied to that terrain slot across all voices.
The panel also shows a small heightmap viewer with the active scan path and moving scan point overlaid.

Under the hood, `TerrainOsc` supports image import through JUCE image loading:

```cpp
bool setHeightmapFromImage(const juce::Image& image);
bool importHeightmapFile(const juce::File& file);
```

JUCE handles the file formats registered by the application, typically including PNG, JPG, and BMP-style bitmap formats. Imported images are converted to normalized bipolar terrain data and capped to the oscillator's maximum imported size.

The processor loads the image on the message thread, briefly suspends audio processing, and then copies the terrain data into every voice's matching terrain oscillator.

## Multi-Instance Behavior

There are eight independent terrain oscillators per voice:

```text
TerrainOsc[8]
ENV_TERRAIN1..ENV_TERRAIN8
```

Each slot owns its own parameters, envelope, phase, orbit-noise state, smoothing state, and heightmap data. Duplicating the terrain oscillator across pages should not share the same control values.
In the oscillator slot layout, Terrain consumes two slot units when there is room. The following slot is skipped by both the UI layout and the voice slot scanner so hidden controls do not keep sounding behind the terrain panel.

## Current Limitations

- No editable path overlay yet.
- No mipmapped / anti-aliased terrain levels yet.
- Terrain parameters are not exposed as modulation-matrix destinations yet.
- Presets do not yet collect imported terrain images as sample dependencies.

## Suggested Next Upgrades

1. Add drag-and-drop image import in the terrain panel.
2. Add mipmapped terrain levels for cleaner high notes.
3. Add modulation destinations for center, radius, ratio, smoothing, and morph.
4. Save terrain image references or embedded terrain data in presets.

## Source Files

- [TerrainOsc.h](../Source/Engine/Oscillators/TerrainOsc.h)
- [TerrainOsc.cpp](../Source/Engine/Oscillators/TerrainOsc.cpp)
- [OscPanel.cpp](../Source/UI/OscPanel.cpp)
- [EngineTests.cpp](../Tests/EngineTests.cpp)
