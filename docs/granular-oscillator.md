# Granular Oscillator

## MVP Scope

`GranularOsc` is the first integrated granular engine pass for GLIESE 710. It is a real-time safe oscillator core with a fixed source buffer and preallocated grain pool. It does not yet include drag-and-drop sample loading, live input capture, region markers, scenes, or sample browser infrastructure.

The identity of this first pass is:

```text
sample source -> preallocated stereo grain cloud -> optional physical body -> synth voice mix
```

## Engine

There are eight independent `GranularOsc` instances per voice. Each instance owns:

| Resource | Purpose |
|---|---|
| `source[65536]` | Fixed mono source buffer, generated at prepare time or filled by `setSample()` |
| `Grain[96]` | Preallocated active grain pool |
| `ModalMode[32]` | Optional physical body/modal resonator bank |
| Trigger clock | Free-cloud, burst, or audio-rate grain spawning |
| Deterministic RNG | Repeatable per-note spray, pan, pitch jitter, and reverse decisions |

The fixed source buffer keeps the audio thread allocation-free. Later sample loading should decode and resample off the audio thread, then swap/copy into the fixed source storage through a safe handoff.

## Parameters

The table shows the first slot prefix. Additional granular slots use `osc_grain2_` through `osc_grain8_`.

| Parameter | Range | Description |
|---|---|---|
| `osc_grain_trigger_mode` | Free Cloud / Burst / Audio Rate | Grain spawning behavior |
| `osc_grain_window` | Hann / Triangle / Gaussian / Tukey / Rect / Exp Attack / Exp Decay | Per-grain amplitude window |
| `osc_grain_position` | 0-1 | Center read position in the source buffer |
| `osc_grain_size` | 5-1000 ms | Grain duration |
| `osc_grain_density` | 0.1-1200 grains/sec | Cloud density or audio-rate trigger intensity |
| `osc_grain_pitch` | -48 to +48 semitones | Grain playback transposition |
| `osc_grain_spray` | 0-1 | Random position radius around the center |
| `osc_grain_jitter` | 0-1 | Timing and pitch instability |
| `osc_grain_pan` | 0-1 | Per-grain stereo spread |
| `osc_grain_reverse` | 0-1 | Reverse probability |
| `osc_grain_scan` | -1 to +1 | Automatic source-position movement |
| `osc_grain_freeze` | 0-1 | Locks grains to the main position when above 0.5 |
| `osc_grain_body_type` | Off / Glass / Metal / Wood / Plate / String / Drum / Pipe / Alien | Optional physical body model |
| `osc_grain_body_root` | -48 to +48 semitones | Modal root relative to played pitch |
| `osc_grain_body_size` | 0-1 | Larger bodies lower the modal bank |
| `osc_grain_body_decay` | 0-1 | Resonance length |
| `osc_grain_body_damp` | 0-1 | Material damping |
| `osc_grain_body_bright` | 0-1 | High-mode emphasis |
| `osc_grain_body_inharm` | 0-1 | Extra inharmonic modal stretch |
| `osc_grain_body_excite` | 0-1 | How hard grains excite the body |
| `osc_grain_body_mix` | 0-1 | Dry grain cloud vs resonated body |
| `osc_grain_body_spread` | 0-1 | Alternating modal stereo spread |
| `osc_grain_body_modes` | 4-32 | Active modal filter count |
| `osc_grain_level` | 0-1 | Engine mix level |
| `osc_grain_tune` | -100 to +100 cents | Fine tune relative to the played note |
| `osc_grain_octave` | -3 to +3 | Pitch offset inherited from the played note |

Each granular slot has its own ADSR envelope through `ENV_GRAN1` through `ENV_GRAN8`.

## Trigger Modes

### Free Cloud

Continuously spawns grains from the density clock. Best for pads, drones, smears, and evolving clouds.

### Burst

Starts a finite grain burst on note-on. Density controls how fast the burst emits.

### Audio Rate

Ties the grain trigger rate to note frequency and density, pushing the grain stream toward oscillator-like behavior.

## Physical Body

The physical body section is optional and defaults to bypass:

```text
body type = Off
body mix  = 0
```

When enabled, the stereo grain cloud excites a fixed modal resonator bank. This keeps the synth granular, while letting grains ring through modeled bodies such as glass, metal, wood, plates, strings, drums, pipes, and alien alloys.

The implementation is a practical modal-resonator MVP: a bank of preallocated band-pass modes with body-specific frequency ratios, damping, brightness tilt, inharmonic stretch, excitation gain, and stereo spread. It is not a finite-difference or circuit-accurate physical simulation.

## Modulation Destinations

The modulation matrix can target:

```text
Gran_Position
Gran_Size
Gran_Density
Gran_Pitch
Gran_Spray
Gran_Freeze
Gran_Level
Gran_BodyMix
Gran_BodySize
Gran_BodyDamping
Gran_BodyInharm
```

These are appended to the modulation destination enum to preserve earlier destination indices.

## Next Upgrades

- Drag-and-drop/sample-browser loading with background decode and resampling
- Stereo sample buffers and true stereo grain reads
- Sample regions, slices, root note, transient markers, and named scenes
- Tempo-synced density divisions
- Scale-quantized pitch clouds
- Grain direction sequencing
- Live input circular buffer and freeze capture
- Visual waveform/grain cloud overlay
- Dual granular layer morphing

Implementation order should keep all file decoding, analysis, and resampling off the audio thread. Completed sample assets should be handed to voices as immutable shared data, following the terrain asset model.
