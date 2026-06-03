# 3D Function Oscillator

## Concept

A standard oscillator evaluates a 1D waveform function:

```
sample = f(phase)
```

The 3D oscillator instead moves a point through a 3D mathematical field and reads the scalar value at each position:

```
(x, y, z) = Path(phase, params)
(x, y, z) = Warp(x, y, z, warpAmount)
(x, y, z) = Fold(x, y, z, foldAmount)
sample     = tanh( Field(x, y, z, fieldFrequency) × drive )
```

The **path** determines how the point moves; its phase speed equals the oscillator pitch. The **field** determines the timbre at each position. **Field frequency** changes harmonic density without changing note pitch. **Warping** bends the coordinate space, **folding** mirrors it back into a bounded region, and **drive** controls the final soft saturation.

## DSP Pipeline (per sample)

```
phase += frequency / sampleRate
t      = phase × 2π

p      = computePath(t)       // → Vec3
p      = warpCoordinates(p)   // domain distortion
p      = foldCoordinates(p)   // optional coordinate reflection
s      = blend(p)             // field A + morph × (field B - field A)
output = tanh(s × drive)      // soft clip to ±1
```

The field is evaluated every `kSubRate` (= 4) audio samples and held between evaluations (zero-order hold). This reduces trig/sqrt cost by ~4× with negligible quality loss for typical musical frequencies.

## Field Types

All fields are evaluated at the warped/folded coordinate `(x, y, z)`. `F` denotes `osc_3d_field_freq`. Parameters `a`, `b`, and `tension` are remapped from `radius`, `depth`, and `morphT` respectively to give each field musical meaning without adding extra knobs.

### Original Geometric Fields

The first ten fields are the original GLIESE mathematical terrains:

```text
SinSum
SinProduct
SphericalRipple
Torus
VortexField
TurbulenceNoise
LorenzAttractor
MagneticDipole
FoldedSpace
ChebyshevMorph
```

They are useful for abstract, animated, and less traditional oscillator colors. The later harmonic fields are tuned toward more musical interval behavior.

### Harmonic (`HarmonicLattice`)
```
sin(x·F) + a·sin(2y·F + z) + b·sin(3z·F + x)
```
Harmonic series layering along three axes. `radius` (→ `a`) weights the second harmonic; `depth` (→ `b`) weights the third. Good starting point — smooth, musical, predictable.

### Chord (`ChordField`)
```
sin(x·F) + a·sin(y·third·F) + b·sin(z·1.5·F)
```
Three axes map to the root (x), third (y), and fifth (z) of a chord. `morphT` slides `third` from 1.25 (major, 5/4) toward 1.2 (minor, 6/5). Use `morph` to smoothly transition between major and minor tonality.

### Shell (`ResonanceShell`)
```
sin(dist · k · F)     dist = √(x²+y²+z²),  k = max(0.5, radius + depth×2)
```
Concentric spherical resonance shells. Strong fundamental at small radius, overtones expand outward. Produces bell-like, glassy tones. `radius` and `depth` control shell spacing.

### Gravity (`IntervalGravity`)
```
sin(|p – root| · F)
+ 0.6 · sin(|p – fifth| · 1.5·F)
+ 0.4 · sin(|p – octave| · 2.0·F)
```
Three gravity wells placed at musically significant positions in 3D space: root at origin, fifth at `(0.7·radius, 0.2, −0.1)`, octave at `(−0.4, 0.6·depth, 0.3)`. The path's distance to each well modulates its contribution. Produces naturally harmonic, beating textures.

### Formant (`FormantVowel`)
```
g₁(x)·sin(x·3F) + g₂(y)·sin(y·6F) + g₃(z)·sin(z·10F)
```
where each `gᵢ` is a Gaussian envelope centered near a vowel formant frequency. `morphT` (tension) slides the Gaussian centers along x and z axes, morphing between vowel characters. `warpAmt` widens the x-axis formant. Use `morph` automation for slow vowel movement.

### Metallic (`MetallicRatio`)
```
sin(x·F) + 0.5·sin(y·r₂·F) + 0.25·sin(z·r₃·F)
```
Blends between integer ratios (2, 3) and irrational ratios (√2 ≈ 1.414, φ ≈ 1.618) via `chaos`. At `chaos = 0`: harmonic. At `chaos = 1`: inharmonic, bell/metallic. Produces predictable changes in metallicity without changing note pitch.

### Tension (`TensionResolution`)
```
stable = sin(x·F) + 0.5·sin(2y·F)
tense  = sin(x·y·z·8·F)
output = stable·(1 − tension) + tense·tension
```
`morphT` (tension) crossfades between a stable harmonic component and a nonlinear cross-modulation product. At 0: warm and consonant. At 1: harsh and unresolved. Excellent for expression-mapped dramatic timbre shifts.

### Body (`BodyEdgeAir`)
```
body = sin(x·F)
edge = sin(x·F + 3·sin(y·F))
air  = sin(8·z·F + x·y·F)
output = body + warpAmt·0.5·edge + chaos·0.3·air
```
A timbral axis model: x maps to body resonance, y adds edge/bow noise via FM-style phase deviation, z adds breathiness/air. `warpAmt` controls the edge character; `chaos` adds airy breath noise. Useful for acoustic instrument emulation.

## Path Types

### Circle
```
x = radius × cos(t)
y = radius × sin(t)
z = depth × sin(t × 0.5 × zRatio)
```
The baseline. Produces a stable orbit. Timbre is determined purely by which region of the field is traversed.

### Spiral
```
r = 0.5 + 0.5 × sin(t/4)
x = r × radius × cos(t)
y = r × radius × sin(t)
z = depth × (fract(t × zRatio / 2π) − 0.5)
```
The orbit expands and contracts. As `r` changes, the oscillator scans a ring of the field rather than a fixed circle.

### Lissajous
```
x = radius × cos(3t + π/2)
y = radius × sin(2t)
z = depth × sin(5t × zRatio)
```
3:2:5 ratio Lissajous figure. Creates a complex closed curve that self-intersects, producing beating and subharmonics.

### RandomWalk
```
vel += chaos × noise()
vel *= 0.995           ← damping (orbit tendency)
pos += vel × dt × 500
pos = tanh(pos/radius) × radius   ← soft limit
```
Brownian motion constrained near the origin. The `chaos` parameter controls velocity variance. At low chaos: smooth organic motion. At high chaos: erratic jumping.

### PhysicsOrbit
```
r = radius × (1 − ecc × cos(t))   where ecc = 0.3 × chaos
x = r × cos(t)
y = r × sin(t)
z = depth × sin(2t × zRatio)
```
Elliptical orbit. Higher `chaos` increases eccentricity — the point swings closer and further from the field center each cycle, causing amplitude modulation-like effects.

## Multi-Instance Slot Model

The current UI exposes up to eight independent 3D oscillator slots. They do not share one ensemble parameter set. Each slot has its own field, path, envelope, level, fine tune, octave, phase state, and parameter prefix:

```text
osc_3d_   # slot 1
osc_3d2_  # slot 2
...
osc_3d8_  # slot 8
```

The legacy `engine_3d_detune` parameter can still be used as a broad detune color when several 3D slots are active, but the primary architecture is now independent slots rather than a fixed six-voice 3D ensemble.

## Parameters

The table below shows the first slot prefix. Additional 3D slots use `osc_3d2_` through `osc_3d8_`.

| Parameter | Range | Effect |
|---|---|---|
| `osc_3d_field` | 0–17 | Which field type |
| `osc_3d_path` | 0–4 | Which path type |
| `osc_3d_radius` | 0.1–4.0 | Orbit size / harmonic weight `a` in some fields |
| `osc_3d_depth` | 0.0–4.0 | Z-axis extent / overtone weight `b` in some fields |
| `osc_3d_warp` | 0.0–2.0 | Domain warp intensity / edge character in Body field |
| `osc_3d_rotation` | 0.0–1.0 | Rotates the orbit in XY plane (0–360°) |
| `osc_3d_chaos` | 0.0–1.0 | Noise injected into path / metallic blend / air in Body field |
| `osc_3d_morph` | 0.0–1.0 | Crossfades to the next field type / tension/vowel in some fields |
| `osc_3d_field_freq` | 0.25–16.0 | Harmonic-density multiplier inside field formulas |
| `osc_3d_z_ratio` | 0.25–4.0 | Multiplies the z-axis path rate while preserving note pitch |
| `osc_3d_fold` | 0.0–1.0 | Blends warped coordinates toward mirrored/folded coordinates |
| `osc_3d_drive` | 0.25–4.0 | Multiplies raw field output before `tanh` |
| `osc_3d_level` | 0.0–1.0 | Output level (summed ensemble output) |
| `osc_3d_tune` | −100–+100 cents | Fine tune relative to the played note |
| `osc_3d_octave` | −3–+3 | Octave transpose |
| `osc_3d_adsr_{a,d,s,r}` | — | Per-oscillator amplitude envelope |

## Domain Warping

Before sampling the field, coordinates are distorted:

```cpp
p.x += warpAmt × sin(p.y × 3.0 + p.z × 1.5)
p.y += warpAmt × cos(p.z × 2.0 + p.x × 1.2)
p.z += warpAmt × sin(p.x × 4.0 + p.y × 0.8)
```

This is the same technique used in procedural graphics to make marble, clouds, and terrain. In audio, it folds the field into itself — low warp adds subtle texture, high warp creates dense overtones and instability.

## Coordinate Folding

After warping, `osc_3d_fold` can mirror each coordinate back into the `[−1, 1]` region:

```cpp
folded = foldCoordinate(p)
p = mix(p, folded, foldAmount)
```

At low values this adds wavefolder-like edge and motion. At high values it creates mirrored regions in the 3D field, especially audible when `radius`, `depth`, or `field_freq` push the scan path outside the central region.

## Field Frequency Vs Pitch

Pitch comes from phase speed:

```cpp
phase += frequency / sampleRate;
```

`osc_3d_field_freq` does not change phase speed. It multiplies the field's internal terms — use it as a brightness or harmonic-density control. High values generate more aliasing and should eventually pair with oscillator oversampling.

## Morph

`morphT` (0–1) linearly crossfades between the current field and the next one in the enum sequence:

```cpp
fieldA = params.field
fieldB = (fieldA + 1) % NumFields

output = fieldA_value + morphT × (fieldB_value - fieldA_value)
```

Automating `morph` allows smooth timbral evolution between adjacent fields.

## Patch Examples

**Warm harmonic chord pad**
```
osc_3d_field = Chord, path = Circle
radius = 1.0, depth = 0.8, morph = 0.3
field_freq = 1.0, z_ratio = 1.0, drive = 1.0
layer extra 3D slots with slight octave/detune changes for ensemble width
```

**Bell/metallic lead**
```
field = Shell, path = Circle
radius = 1.2, depth = 0.7, field_freq = 2.5
z_ratio = 1.0, drive = 1.5, chaos = 0.2
use one 3D slot for the cleanest attack
```

**Organic vowel pad**
```
field = Formant, path = RandomWalk
radius = 1.0, depth = 1.0, warp = 0.3
chaos = 0.15, morph = 0.5, field_freq = 1.5
duplicate into another 3D slot with different rotation for slow stereo movement
```

**Tense digital texture**
```
field = Tension, path = Lissajous
morph = 0.7, field_freq = 4.0, warp = 0.4
fold = 0.3, drive = 2.0, z_ratio = 2.0
```

**Acoustic body emulation**
```
field = Body, path = PhysicsOrbit
warp = 0.4, chaos = 0.2, radius = 1.0
depth = 0.8, field_freq = 1.5, drive = 1.2
```

## Modulation Examples

```
LFO2       → Osc3D_Morph     amount +0.60  bipolar on   ← evolving field crossfade
Aftertouch → Osc3D_Fold      amount +0.35  bipolar off  ← pressure adds digital edge
Velocity   → Osc3D_Drive     amount +0.40  bipolar off  ← harder = more saturation
Macro1     → Osc3D_FieldFreq amount +0.75  bipolar off  ← macro controls brightness
Env_3D     → Osc3D_Warp      amount +0.50  bipolar off  ← warp tracks envelope
```

## Extending the Field Library

To add a new field:

1. Add an entry to `FieldType` enum in [Osc3D.h](../Source/Engine/Oscillators/Osc3D.h)
2. Add a case to `evaluateField()` in [Osc3D.cpp](../Source/Engine/Oscillators/Osc3D.cpp)
3. Update the `osc_3d_field` parameter range in `PluginProcessor.cpp` (upper bound = NumFields − 1)
4. Add the name to the `modeBoxA` item list in [OscPanel.cpp](../Source/UI/OscPanel.cpp)
5. Update this doc with the formula

No other changes needed — morph, mod matrix destinations, and APVTS all adapt automatically.
