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
sample     = tanh( Field(x, y, z) )
```

The **path** determines how the point moves — its speed equals the oscillator pitch. The **field** determines the timbre at each position. **Warping** distorts the coordinate space before sampling, adding texture and movement.

## DSP Pipeline (per sample)

```
phase += frequency / sampleRate
t      = phase × 2π

p      = computePath(t)       // → Vec3
p      = warpCoordinates(p)   // domain distortion
s      = blend(p)             // field A + morph × (field B - field A)
output = tanh(s)              // soft clip to ±1
```

## Field Types

All fields are evaluated at the warped coordinate `(x, y, z)`.

### SinSumXYZ
```
sin(4x) + cos(3y) + sin(5z)
```
Rich harmonic content. Smooth and musical. Good starting point.

### SinProduct
```
sin(x·y·2) + cos(3z)
```
Cross-modulation between axes creates FM-like sidebands.

### SphericalRipple
```
sin(6r) / r     where r = √(x²+y²+z²)
```
Concentric shells of sound. Strong fundamental at low radius, overtones decay with distance. Creates metallic, bell-like tones.

### Torus
```
dist = √(x²+y²) - 0.7
d    = √(dist² + z²) - 0.3
sin(d × 4π)
```
The oscillator scans the surface of a torus. Produces two strong harmonics with ring-modulation-like interaction.

### VortexField
```
v = (y-z, z-x, x-y)   ← curl-like operator
sin(vx×2) + cos(vy×3)
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
x = radius × cos(t)
y = radius × sin(t)
z = depth × sin(t/2)
```
The baseline. Produces a stable orbit. Timbre is determined purely by which region of the field is traversed.

### Spiral
```
r = 0.5 + 0.5 × sin(t/4)
x = r × radius × cos(t)
y = r × radius × sin(t)
z = depth × fract(t/2π)
```
The orbit expands and contracts. As `r` changes, the oscillator scans a ring of the field rather than a fixed circle.

### Lissajous
```
x = radius × cos(3t + π/2)
y = radius × sin(2t)
z = depth × sin(5t)
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
r = radius × (1 - ecc × cos(t))   where ecc = 0.3 × chaos
x = r × cos(t)
y = r × sin(t)
z = depth × sin(2t)
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

## Morph

`morphT` (0–1) linearly crossfades between the current field and the next one in the enum sequence:

```cpp
fieldA = params.field
fieldB = (fieldA + 1) % NumFields

output = fieldA_value + morphT × (fieldB_value - fieldA_value)
```

Automating `morph` allows smooth timbral evolution between any two adjacent fields.

## Anti-aliasing (Phase 6)

The 3D oscillator is inherently non-bandlimited — the field functions can produce arbitrarily high harmonics. Phase 6 will add 2× oversampling via `juce::dsp::Oversampling` after the tanh stage. This halves the aliasing floor at the cost of 2× CPU per voice.

## Extending the Field Library

To add a new field:

1. Add an entry to `FieldType` enum in [Osc3D.h](../Source/Engine/Oscillators/Osc3D.h)
2. Add a case to `evaluateField()` in [Osc3D.cpp](../Source/Engine/Oscillators/Osc3D.cpp)
3. Update the `NumFields` count (it's the last enum value — automatic)
4. Add to the `osc_3d_field` parameter choice list in `PluginProcessor.cpp`
5. Update [docs/oscillator-3d.md](oscillator-3d.md) with the formula

No other changes needed — morph, mod matrix destinations, and APVTS all adapt automatically.
