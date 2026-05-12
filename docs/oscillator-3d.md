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
sample     = tanh( Field(x, y, z, fieldFrequency) * drive )
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

## Field Types

All fields are evaluated at the warped coordinate `(x, y, z)`.

### SinSumXYZ
```
sin(4xF) + cos(3yF) + sin(5zF)
```
Rich harmonic content. Smooth and musical. Good starting point. `F` is `osc_3d_field_freq`, a brightness/harmonic-density multiplier.

### SinProduct
```
sin(x·y·2F) + cos(3zF)
```
Cross-modulation between axes creates FM-like sidebands.

### SphericalRipple
```
sin(6rF) / r     where r = sqrt(x*x + y*y + z*z)
```
Concentric shells of sound. Strong fundamental at low radius, overtones decay with distance. Creates metallic, bell-like tones.

### Torus
```
dist = √(x²+y²) - 0.7
d    = √(dist² + z²) - 0.3
sin(d × 4πF)
```
The oscillator scans the surface of a torus. Produces two strong harmonics with ring-modulation-like interaction.

### VortexField
```
v = (y-z, z-x, x-y)   ← curl-like operator
sin(vx×2F) + cos(vy×3F)
```
Chaotic but structured. Sounds like FM with slow beating.

### TurbulenceNoise
```
n = fract(sin(12.9x + 4.1y) × 43758.5)
```
Trig-hash value noise. Produces aperiodic, grainy textures. Good for pads and atmospheres.

### LorenzAttractor
```
r = distance from Lorenz centroid (0, 0, 27)
sin(r × 0.5)
```
The path of the Lorenz strange attractor is embedded in the field distance. Produces slow, organic beating.

### MagneticDipole
```
(3cos²θ - 1) / r³     where θ = arccos(z/r)
```
Magnetic dipole B-field magnitude. Strong axis-aligned harmonics, falls off with cube of distance.

### FoldedSpace
```
4 iterations of: p = |p| - 0.7
then: sin(5x) + cos(5y) + sin(5z)
```
Abs-folding creates a Sierpinski-like fractal. Produces dense, sharp overtones.

### ChebyshevMorph
```
T4(x) + T5(y) + T6(z)
```
Chebyshev polynomials of the 4th, 5th, and 6th order. Creates predictable harmonic spectra — T_n produces n harmonics. Coordinates clamped to [-1, 1].

## Path Types

### Circle
```
x = radius * cos(t)
y = radius * sin(t)
z = depth * sin(t * 0.5 * zRatio)
```
The baseline. Produces a stable orbit. Timbre is determined purely by which region of the field is traversed.

### Spiral
```
r = 0.5 + 0.5 * sin(t/4)
x = r * radius * cos(t)
y = r * radius * sin(t)
z = depth * (fract(t * zRatio / 2π) - 0.5)
```
The orbit expands and contracts. As `r` changes, the oscillator scans a ring of the field rather than a fixed circle.

### Lissajous
```
x = radius * cos(3t + π/2)
y = radius * sin(2t)
z = depth * sin(5t * zRatio)
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
r = radius * (1 - ecc * cos(t))   where ecc = 0.3 * chaos
x = r * cos(t)
y = r * sin(t)
z = depth * sin(2t * zRatio)
```
Elliptical orbit. Higher `chaos` increases eccentricity — the point swings closer and further from the field center each cycle, causing amplitude modulation-like effects.

## Parameters

| Parameter | Range | Effect |
|---|---|---|
| `osc_3d_field` | 0–9 | Which field type |
| `osc_3d_path` | 0–4 | Which path type |
| `osc_3d_radius` | 0.1–4.0 | Orbit size — controls which spatial region is traversed |
| `osc_3d_depth` | 0.0–4.0 | Z-axis extent — adds z-dimensional variation |
| `osc_3d_warp` | 0.0–2.0 | Domain warp intensity — folds/twists coordinate space |
| `osc_3d_rotation` | 0.0–1.0 | Rotates the orbit in XY plane (normalized 0–360°) |
| `osc_3d_chaos` | 0.0–1.0 | Noise injected into path coordinates |
| `osc_3d_morph` | 0.0–1.0 | Crossfades to the next field type |
| `osc_3d_field_freq` | 0.25–16.0 | Harmonic-density multiplier inside field formulas |
| `osc_3d_z_ratio` | 0.25–4.0 | Multiplies the z-axis path rate while preserving note pitch |
| `osc_3d_fold` | 0.0–1.0 | Blends warped coordinates toward mirrored/folded coordinates |
| `osc_3d_drive` | 0.25–4.0 | Multiplies raw field output before `tanh` |
| `osc_3d_level` | 0.0–1.0 | Output level |
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

After warping, `osc_3d_fold` can mirror each coordinate back into the `[-1, 1]` region:

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

`osc_3d_field_freq` does not change phase speed. It multiplies the field's internal terms:

```cpp
sin(x * 4 * fieldFreq) + cos(y * 3 * fieldFreq) + sin(z * 5 * fieldFreq)
```

Use it like a brightness or harmonic-density control. High values are intentionally more aggressive and should eventually pair with oscillator oversampling.

## Morph

`morphT` (0–1) linearly crossfades between the current field and the next one in the enum sequence:

```cpp
fieldA = params.field
fieldB = (fieldA + 1) % NumFields

output = fieldA_value + morphT × (fieldB_value - fieldA_value)
```

Automating `morph` allows smooth timbral evolution between any two adjacent fields.

## Anti-Aliasing

The 3D oscillator is inherently non-bandlimited. High `field_freq`, coordinate folding, heavy drive, noise fields, and fast warp can all generate high harmonics. Oversampling remains planned for a quality mode; until then, patches should keep extreme settings lower for cleaner pitched sounds.

## Extending the Field Library

To add a new field:

1. Add an entry to `FieldType` enum in [Osc3D.h](../Source/Engine/Oscillators/Osc3D.h)
2. Add a case to `evaluateField()` in [Osc3D.cpp](../Source/Engine/Oscillators/Osc3D.cpp)
3. Update the `NumFields` count (it's the last enum value — automatic)
4. Add to the `osc_3d_field` parameter choice list in `PluginProcessor.cpp`
5. Update [docs/oscillator-3d.md](oscillator-3d.md) with the formula

No other changes needed — morph, mod matrix destinations, and APVTS all adapt automatically.
