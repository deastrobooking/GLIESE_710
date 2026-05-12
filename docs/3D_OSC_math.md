# Mathematical Guide for a 3D Function Oscillator

A normal oscillator is usually a one-dimensional function of phase:

```text
sample = f(phase)
```

For example:

```text
sine = sin(phase)
saw = phase mapped from -1 to 1
square = sign(sin(phase))
```

A **3D function oscillator** expands this idea. Instead of reading one value from one phase, we move through a 3D mathematical space and sample a function at that position.

```text
sample = F(x, y, z)
```

The key idea is:

```text
phase drives motion
motion creates a 3D coordinate
the coordinate is evaluated by a function
the function value becomes audio
```

So the oscillator becomes:

```text
phase → path through 3D space → function evaluation → audio sample
```

---

# 1. Phase: The Core of Every Oscillator

Every oscillator starts with phase.

Phase is a normalized position in the waveform cycle:

```text
phase = 0.0 to 1.0
```

Each audio sample advances the phase:

```text
phase += frequency / sampleRate
```

When phase reaches 1.0, it wraps around:

```text
if phase >= 1.0:
    phase -= 1.0
```

In radians, one full cycle is:

```text
angle = phase * 2π
```

\theta = 2\pi\phi

Where:

```text
φ = normalized phase from 0 to 1
θ = phase angle in radians
```

For a 440 Hz oscillator at 48,000 Hz sample rate:

```text
phaseIncrement = 440 / 48000
phaseIncrement = 0.0091666...
```

That means the oscillator completes 440 cycles per second.

---

# 2. From 1D Phase to 3D Coordinates

The main leap is this:

```text
Instead of using phase directly as the waveform,
use phase to generate a point in 3D space.
```

For example:

```text
x = sin(θ)
y = cos(θ)
z = sin(θ * 0.5)
```

Now every sample gives us a 3D point:

```text
p = (x, y, z)
```

A basic circular 3D path looks like this:

```text
x = r * cos(θ)
y = r * sin(θ)
z = d * sin(θ * zRatio)
```

Where:

```text
r = radius
d = depth
zRatio = z-axis movement ratio
```

This produces a point that travels around a circle while moving up and down in the z-axis.

---

# 3. The 3D Function

Once we have a coordinate, we evaluate a function.

Example:

```text
F(x, y, z) = sin(4x) + cos(3y) + sin(5z)
```

The oscillator output is:

```text
sample = F(x, y, z)
```

But because this can exceed `-1.0` to `1.0`, we usually normalize or saturate it:

```text
sample = tanh(F(x, y, z))
```

A simple 3D oscillator is:

```text
x = cos(θ)
y = sin(θ)
z = sin(θ * 0.5)

raw = sin(4x) + cos(3y) + sin(5z)

sample = tanh(raw)
```

This is already more expressive than a sine oscillator because the waveform is produced by traveling through a spatial function.

---

# 4. Scalar Fields

A scalar field is a function that returns one value from a position.

```text
F(x, y, z) → number
```

In audio terms, that number becomes the sample.

Examples:

```text
F(x, y, z) = sin(x)
F(x, y, z) = sin(x + y + z)
F(x, y, z) = sin(x * y)
F(x, y, z) = cos(sqrt(x² + y² + z²))
```

A scalar field is the easiest version of the 3D oscillator to implement.

The general model is:

s(t) = F(x(t), y(t), z(t))

Where:

```text
s(t) = audio sample over time
F = 3D function
x(t), y(t), z(t) = moving coordinate path
```

For developers, this means your oscillator has two separate systems:

```text
Path generator
Function evaluator
```

That separation is important.

---

# 5. Path Generator

The path generator decides where the point is in 3D space.

Common path types:

```text
circle
spiral
helix
Lissajous curve
random walk
Bezier path
attractor path
MIDI-controlled path
envelope-controlled path
```

## Circular Path

```text
x = r * cos(θ)
y = r * sin(θ)
z = 0
```

This is like scanning around a flat circle.

## Helix Path

```text
x = r * cos(θ)
y = r * sin(θ)
z = depth * phase
```

This moves around while rising through space.

## Lissajous Path

```text
x = sin(aθ + px)
y = sin(bθ + py)
z = sin(cθ + pz)
```

Where:

```text
a, b, c = frequency ratios
px, py, pz = phase offsets
```

x=\sin(a\theta+p_x),\quad y=\sin(b\theta+p_y),\quad z=\sin(c\theta+p_z)

This is extremely useful for sound design because small changes to `a`, `b`, and `c` create big timbral changes.

Example:

```text
x = sin(1θ)
y = sin(2θ)
z = sin(3θ)
```

This gives the oscillator a harmonic relationship.

Example with non-integer ratios:

```text
x = sin(1.0θ)
y = sin(1.414θ)
z = sin(2.718θ)
```

This creates more complex, inharmonic, metallic, or evolving sounds.

---

# 6. Function Frequency vs Oscillator Frequency

This is one of the most important concepts.

The oscillator frequency controls how fast phase moves.

But the 3D function can also contain multipliers:

```text
F(x, y, z) = sin(8x) + cos(5y)
```

Those multipliers affect the complexity of the waveform.

```text
sin(x)     smoother
sin(8x)    sharper and brighter
sin(32x)   much more complex, more aliasing risk
```

So there are two different “frequency-like” systems:

```text
1. Phase frequency: musical pitch
2. Function frequency: harmonic density / brightness / complexity
```

In synth UI terms:

```text
Oscillator Frequency = note pitch
Field Frequency = timbre brightness
```

This distinction matters because high function multipliers can generate high harmonics even if the played note is low.

---

# 7. Distance Functions

A powerful class of 3D functions is based on distance.

Distance from the origin:

d = \sqrt{x^2 + y^2 + z^2}

Then use distance inside a waveform:

```text
F(x, y, z) = sin(d * amount)
```

This creates spherical wave behavior.

Example:

```text
d = sqrt(x*x + y*y + z*z)
sample = sin(d * 10.0)
```

Distance-based fields are great for:

```text
glassy tones
bells
drones
spatial movement
organic modulation
```

You can also measure distance from a point:

```text
d = sqrt((x - cx)² + (y - cy)² + (z - cz)²)
```

Where:

```text
cx, cy, cz = center point
```

That lets you create “sound gravity wells” or “harmonic hotspots.”

---

# 8. Dot Product as a Sound Tool

The dot product measures how much two vectors point in the same direction.

a \cdot b = a_xb_x + a_yb_y + a_zb_z

For a 3D oscillator, this can be used to project movement onto a direction.

Example:

```text
direction = normalize(Vec3(1, 0.5, 0.25))
value = dot(position, direction)
sample = sin(value * harmonicAmount)
```

This is like scanning the 3D field from a certain angle.

If you modulate the direction vector, the sound changes as if the oscillator is rotating through the function.

Useful controls:

```text
Scan Angle X
Scan Angle Y
Scan Angle Z
Projection Amount
Rotation
```

This gives you a very musical concept:

```text
Changing the scan direction changes the harmonic content.
```

---

# 9. Cross Product for Motion and Rotation

The cross product returns a vector perpendicular to two input vectors.

For audio, you probably will not use it directly as an output as often, but it is useful for movement systems.

Example uses:

```text
rotating a path around an axis
creating orbital motion
building vector fields
making chaotic swirling paths
```

For a synth, cross product math becomes useful when you want the oscillator point to orbit around a direction vector, almost like a particle in a magnetic field.

---

# 10. Coordinate Warping

Coordinate warping means you change the coordinate before evaluating the function.

Instead of:

```text
sample = F(x, y, z)
```

You do:

```text
x2 = x + warpX(x, y, z)
y2 = y + warpY(x, y, z)
z2 = z + warpZ(x, y, z)

sample = F(x2, y2, z2)
```

Example:

```text
x2 = x + warpAmount * sin(y * 3.0)
y2 = y + warpAmount * cos(z * 4.0)
z2 = z + warpAmount * sin(x * 5.0)
```

This is one of the most important parts of the sound engine.

Coordinate warping gives you:

```text
movement
instability
complexity
digital grit
evolving harmonics
nonlinear timbre
```

In code:

```cpp
Vec3 warp(Vec3 p, float amount)
{
    Vec3 out = p;

    out.x += amount * std::sin(p.y * 3.0f + p.z);
    out.y += amount * std::cos(p.z * 4.0f + p.x);
    out.z += amount * std::sin(p.x * 5.0f + p.y);

    return out;
}
```

Then:

```cpp
Vec3 p = path(phase);
Vec3 warped = warp(p, warpAmount);
float sample = field(warped);
```

Coordinate warping is similar to what procedural graphics use for marble, clouds, fire, terrain, and fluid-like movement.

---

# 11. Function Composition

Function composition means feeding one function into another.

Example:

```text
A = sin(x * 3.0)
B = cos(y * 4.0)
C = sin(A + B + z)
```

Then:

```text
sample = C
```

This creates nested behavior.

Simple:

```text
sample = sin(x)
```

More complex:

```text
sample = sin(sin(x * 3.0) + cos(y * 4.0) + z)
```

Very complex:

```text
sample = sin(sin(x * y * 4.0) + cos(z + sin(x * 2.0)))
```

This is where your oscillator starts to feel like FM, phase distortion, and wavetable synthesis at the same time.

---

# 12. Modulation as Math

In a normal synth, modulation usually changes parameters:

```text
LFO → filter cutoff
Envelope → amplitude
Velocity → brightness
```

In a 3D function oscillator, modulation can change the math itself.

You can modulate:

```text
path radius
path depth
path ratio
field multiplier
field rotation
warp amount
function blend
distance center
noise amount
fold amount
normalization
chaos amount
```

Example:

```text
radius = baseRadius + envelope * radiusEnvAmount
warp = baseWarp + LFO * warpDepth
zRatio = baseZRatio + aftertouch * zMod
```

This gives you a very powerful idea:

```text
Modulation changes the space, not just the sound.
```

---

# 13. Function Blending

You can blend between multiple functions.

```text
F1 = sin(x * 4.0) + cos(y * 2.0)
F2 = sin(x * y * z * 5.0)
F3 = cos(sqrt(x² + y² + z²) * 8.0)

sample = mix(F1, F2, blend)
```

A two-function blend:

```text
sample = F1 * (1 - blend) + F2 * blend
```

A three-function blend:

```text
sample = F1 * a + F2 * b + F3 * c
```

Where:

```text
a + b + c = 1
```

This could become a very musical front-panel control:

```text
Field Morph
```

Or:

```text
Sine Field ↔ Distance Field ↔ Chaos Field
```

---

# 14. Folding

Folding is a nonlinear shaping technique. It reflects values back when they exceed a boundary.

Basic wavefolding:

```text
if x > 1:
    x = 2 - x

if x < -1:
    x = -2 - x
```

In a 3D oscillator, you can fold either the final audio signal or the coordinates.

## Signal Folding

```text
sample = fold(sample)
```

This sounds like classic wavefolding.

## Coordinate Folding

```text
x = fold(x)
y = fold(y)
z = fold(z)
sample = F(x, y, z)
```

Coordinate folding is more unique. It creates mirrored regions in the 3D function space.

That could become a major sound design feature.

Controls:

```text
Fold X
Fold Y
Fold Z
Fold Amount
Fold Symmetry
```

---

# 15. Rotation Matrices

To rotate the 3D path or field, use rotation matrices.

Rotation around the z-axis:

\begin{bmatrix}x'\y'\z'\end{bmatrix}=\begin{bmatrix}\cos\alpha&-\sin\alpha&0\\sin\alpha&\cos\alpha&0\0&0&1\end{bmatrix}\begin{bmatrix}x\y\z\end{bmatrix}

For developers, the code is:

```cpp
Vec3 rotateZ(Vec3 p, float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);

    return {
        p.x * c - p.y * s,
        p.x * s + p.y * c,
        p.z
    };
}
```

Rotation is useful because it lets the user move through the same function from different angles.

A good synth control would be:

```text
Field Rotation
```

You can make rotation audio-rate, LFO-rate, envelope-controlled, or MPE-controlled.

---

# 16. Noise Fields

A 3D noise field is a function like:

```text
noise(x, y, z)
```

This returns a smooth pseudo-random value based on the coordinate.

Useful types:

```text
value noise
Perlin noise
simplex noise
cellular noise
fractal Brownian motion
```

For audio, raw noise can be harsh, but smooth 3D noise is great for modulation and texture.

Example:

```text
n = noise(x * scale, y * scale, z * scale)
sample = sin(base + n * noiseAmount)
```

Noise can control:

```text
coordinate warp
function blend
amplitude
phase distortion
filter cutoff
grain position
field density
```

A practical field:

```text
F(x, y, z) = sin(x * 4.0 + noise(x, y, z) * amount)
```

This gives a controllable organic digital sound.

---

# 17. Vector Field Oscillator

A scalar field returns one value.

A vector field returns a vector:

```text
V(x, y, z) = (vx, vy, vz)
```

You can use this in two ways.

## Method 1: Use the Vector as Audio

```text
sample = vx
sample = vy
sample = magnitude(V)
sample = dot(V, scanDirection)
```

Magnitude:

```text
m = sqrt(vx² + vy² + vz²)
```

## Method 2: Use the Vector to Move the Point

This is more interesting.

```text
position += V(position) * speed
```

The oscillator becomes a particle moving through a vector field.

Example vector field:

```text
vx = sin(y)
vy = cos(z)
vz = sin(x)
```

Then:

```text
p.x += vx * speed
p.y += vy * speed
p.z += vz * speed
```

This can create chaotic, fluid, unstable, or attractor-like behavior.

Important: this type of oscillator may not naturally repeat every cycle. That can be good for drones and textures, but dangerous for predictable pitch.

---

# 18. Attractors

An attractor is a system where a point evolves over time according to equations.

A famous example is the Lorenz attractor:

```text
dx/dt = σ(y - x)
dy/dt = x(ρ - z) - y
dz/dt = xy - βz
```

This is probably too unstable for a basic pitched oscillator, but very useful as a modulation source.

Use attractors for:

```text
chaotic LFO
motion path
timbre drift
texture movement
effect modulation
```

For pitched oscillators, keep attractors secondary. Use phase-based paths for stable pitch.

---

# 19. Keeping Pitch Stable

This is critical.

A pitched oscillator needs a repeating cycle.

If the 3D path does not repeat cleanly, the pitch may become unstable.

Stable pitch usually requires:

```text
phase-based path
periodic functions
integer or controlled ratios
bounded output
consistent reset behavior
```

Stable:

```text
x = sin(θ)
y = sin(2θ)
z = sin(3θ)
```

Less stable:

```text
x = sin(1.414θ)
y = sin(2.718θ)
z = sin(3.14159θ)
```

The second can sound cool, but it may not loop cleanly over one cycle.

For musical control, you could have two modes:

```text
Harmonic Mode
Free Mode
```

## Harmonic Mode

Ratios snap to musical/integer values:

```text
1:1
1:2
2:3
3:4
5:7
```

This is better for bass, leads, and tuned patches.

## Free Mode

Ratios are continuous:

```text
1.231
2.719
4.113
```

This is better for drones, metallic sounds, FX, and experimental textures.

---

# 20. Aliasing

Aliasing happens when your oscillator generates frequencies above Nyquist.

Nyquist frequency:

```text
nyquist = sampleRate / 2
```

At 48 kHz sample rate:

```text
nyquist = 24 kHz
```

The problem: nonlinear math creates lots of harmonics.

These can cause aliasing:

```text
high field multipliers
wavefolding
hard clipping
sharp discontinuities
chaotic motion
audio-rate coordinate warping
noise at audio rate
```

To reduce aliasing:

```text
oversample the oscillator
limit function multipliers
smooth discontinuities
use tanh instead of hard clip
band-limit harsh functions
avoid naive square/saw discontinuities
low-pass after nonlinear stages
```

For this synth, oversampling should be part of the design.

Good options:

```text
2x oversampling for normal mode
4x oversampling for high quality
8x oversampling for extreme nonlinear patches
```

---

# 21. Normalization

Your function may produce values outside the audio range.

Example:

```text
F = sin(x) + cos(y) + sin(z)
```

Each term can be between `-1` and `1`, so the total can be between `-3` and `3`.

You need output control.

Options:

## Divide by Known Maximum

```text
sample = F / 3.0
```

Clean and predictable.

## Soft Saturation

```text
sample = tanh(F)
```

Musical and forgiving.

## Adaptive Normalization

Track recent peak level and scale output.

Useful but can affect dynamics.

## Hybrid

```text
sample = tanh(F * drive) * outputGain
```

This is probably best for a synth.

Controls:

```text
Drive
Normalize
Output Gain
Limiter
```

---

# 22. Coordinate Ranges

Your function behavior depends heavily on coordinate scale.

If:

```text
x, y, z range from -1 to 1
```

Then:

```text
sin(x)
```

does not complete a full sine cycle.

But:

```text
sin(x * 2π)
```

does.

This is a common mistake.

For audio, you often want to scale coordinates:

```text
sample = sin(x * fieldFrequency * 2π)
```

So the function should usually include:

```text
2π
```

Example:

```cpp
float field(Vec3 p, float fieldFreq)
{
    constexpr float twoPi = 6.28318530718f;

    return std::sin(p.x * fieldFreq * twoPi)
         + std::cos(p.y * fieldFreq * twoPi)
         + std::sin(p.z * fieldFreq * twoPi);
}
```

---

# 23. Developer-Friendly Core Formula

The core architecture can be described like this:

```text
phase = phase + frequency / sampleRate

position = path(phase, pathParams)

position = transform(position, transformParams)

position = warp(position, warpParams)

raw = field(position, fieldParams)

sample = shape(raw, outputParams)
```

In code:

```cpp
float processSample()
{
    phase += frequency / sampleRate;

    if (phase >= 1.0f)
        phase -= 1.0f;

    Vec3 p = pathGenerator(phase);
    p = rotate(p);
    p = warp(p);

    float raw = evaluateField(p);
    float out = std::tanh(raw * drive) * gain;

    return out;
}
```

That is the foundation.

---

# 24. Suggested Math Modules

For the actual synth engine, I would split the math into modules.

## Vec3

```cpp
struct Vec3
{
    float x;
    float y;
    float z;
};
```

Add:

```text
add
subtract
multiply by scalar
dot
cross
length
normalize
distance
rotate
```

## PathGenerator

```cpp
class PathGenerator
{
public:
    Vec3 circular(float phase);
    Vec3 helix(float phase);
    Vec3 lissajous(float phase);
    Vec3 spiral(float phase);
};
```

## FieldEvaluator

```cpp
class FieldEvaluator
{
public:
    float sineField(Vec3 p);
    float distanceField(Vec3 p);
    float productField(Vec3 p);
    float noiseField(Vec3 p);
    float hybridField(Vec3 p);
};
```

## CoordinateWarp

```cpp
class CoordinateWarp
{
public:
    Vec3 sineWarp(Vec3 p, float amount);
    Vec3 noiseWarp(Vec3 p, float amount);
    Vec3 foldWarp(Vec3 p, float amount);
};
```

## OutputShaper

```cpp
class OutputShaper
{
public:
    float normalize(float x);
    float softClip(float x);
    float fold(float x);
    float saturate(float x);
};
```

This keeps the synth flexible.

---

# 25. First Prototype Formula

For the first prototype, use a stable, musical formula.

```cpp
float processSample()
{
    constexpr float twoPi = 6.28318530718f;

    phase += frequency / sampleRate;

    if (phase >= 1.0f)
        phase -= 1.0f;

    float theta = phase * twoPi;

    Vec3 p;
    p.x = std::cos(theta);
    p.y = std::sin(theta);
    p.z = std::sin(theta * zRatio);

    p.x += warpAmount * std::sin(p.y * 3.0f + p.z);
    p.y += warpAmount * std::cos(p.z * 4.0f + p.x);
    p.z += warpAmount * std::sin(p.x * 5.0f + p.y);

    float raw =
        std::sin(p.x * fieldFreq * twoPi) +
        std::cos(p.y * fieldFreq * twoPi) +
        std::sin(p.z * fieldFreq * twoPi);

    float out = std::tanh(raw * drive) * gain;

    return out;
}
```

Good starting values:

```text
zRatio = 0.5
warpAmount = 0.1
fieldFreq = 1.0 to 8.0
drive = 0.5 to 2.0
gain = 0.5
```

For a more harmonically stable version:

```text
zRatio = 1.0, 2.0, or 3.0
```

For a more evolving version:

```text
zRatio = 0.25, 0.5, 1.5, 2.5
```

---

# 26. Important Sound Design Parameters

These are the parameters I would expose first.

```text
Pitch
Field Frequency
Path Radius
Z Depth
Z Ratio
Warp Amount
Warp Rate
Field Rotation
Function Blend
Drive
Output Gain
Oversampling Quality
Harmonic Lock
```

Then advanced parameters:

```text
X Ratio
Y Ratio
Z Ratio
X Phase Offset
Y Phase Offset
Z Phase Offset
Coordinate Fold
Distance Center X
Distance Center Y
Distance Center Z
Noise Scale
Noise Amount
Vector Field Strength
Chaos Amount
```

---

# 27. How This Relates to Existing Synthesis

This 3D oscillator can borrow ideas from several synthesis methods.

## Wavetable Synthesis

Instead of scanning a wavetable, you scan a mathematical field.

```text
wavetable = stored waveform
3D function oscillator = generated waveform field
```

## FM Synthesis

Function composition and coordinate warping can produce FM-like behavior.

```text
sin(x + sin(y))
```

is similar in spirit to phase modulation.

## Physical Modeling

Vector fields and particle movement can behave like simulated motion.

## Granular Synthesis

A voxel field could be scanned like a cloud of tiny sound fragments.

## Additive Synthesis

Multiple field terms can act like partials:

```text
sin(x * 1) + sin(y * 2) + sin(z * 3)
```

## Wavefolding

Coordinate folding and signal folding can create complex nonlinear tones.

---

# 28. The Mental Model

The easiest way to understand this synth is:

```text
A normal oscillator draws a line through time.
A 3D oscillator flies through a mathematical sculpture.
The sound is the value of the sculpture at each point.
```

Or:

```text
Path = how you move
Field = what space sounds like
Warp = how space bends
Shape = how the output is controlled
```

That gives us four major synthesis blocks:

```text
1. Path
2. Field
3. Warp
4. Shape
```

This is probably the cleanest architecture for the plugin.

---

# 29. Recommended Development Order

Build it in this order:

```text
1. Basic phase oscillator
2. 3D circular path
3. Simple sine/cosine scalar field
4. Output normalization with tanh
5. Lissajous path
6. Field frequency control
7. Coordinate warp
8. Function blend
9. Rotation
10. Oversampling
11. Noise field
12. Coordinate folding
13. Vector field modulation
14. GUI visualizer
```

The visualizer matters because this synth concept is spatial. Developers and musicians should be able to see:

```text
the 3D path
the function field
the current scan point
the modulation movement
```

---

# 30. The Core Math Summary

The whole oscillator can be summarized as:

```text
φ[n] = φ[n - 1] + f / Fs
θ[n] = 2πφ[n]
p[n] = path(θ[n])
p'[n] = warp(transform(p[n]))
raw[n] = F(p'[n])
sample[n] = shape(raw[n])
```

Where:

```text
φ = phase
f = note frequency
Fs = sample rate
θ = angle in radians
p = 3D position
F = scalar field
shape = normalization/saturation/folding
```

---

# 31. How GLIESE 710 Implements This

The current implementation lives in:

```text
Source/Engine/Oscillators/Osc3D.h
Source/Engine/Oscillators/Osc3D.cpp
Source/Engine/SynthVoice.cpp
Source/UI/OscPanel.cpp
```

The per-sample signal path is now:

```cpp
Vec3 p = computePath(phase * twoPi);
p = warpCoordinates(p);
p = foldCoordinates(p);
float raw = blend(p);
float sample = tanh(raw * drive);
```

So the math guide maps to the synth like this:

```text
pitch              = phase increment set by MIDI note + octave + pitch modulation
path               = osc_3d_path
field              = osc_3d_field
radius             = osc_3d_radius
z depth            = osc_3d_depth
field frequency    = osc_3d_field_freq
z movement ratio   = osc_3d_z_ratio
coordinate warp    = osc_3d_warp
coordinate fold    = osc_3d_fold
output shaping     = osc_3d_drive
field blend        = osc_3d_morph
```

The important pitch-stability decision is that `osc_3d_field_freq` does not advance phase. It only multiplies terms inside the scalar field:

```cpp
sin(x * 4 * fieldFreq)
cos(y * 3 * fieldFreq)
sin(z * 5 * fieldFreq)
```

That means a 440 Hz note stays a 440 Hz note while the field becomes denser, brighter, or more metallic.

## Implemented Field Examples

GLIESE currently exposes ten scalar fields:

```text
SinSumXYZ       sin(4xF) + cos(3yF) + sin(5zF)
SinProduct      sin(x*y*2F) + cos(3zF)
SphericalRipple sin(6rF) / r
Torus           sin(torusDistance * 4πF)
VortexField     sin((y-z)2F) + cos((z-x)3F)
TurbulenceNoise trig-hash value noise
LorenzAttractor sin(distanceFromLorenzCenter * 0.5F)
MagneticDipole  dipole magnitude scaled by sqrt(F)
FoldedSpace     repeated abs fold, then sine/cosine field
ChebyshevMorph  T4(x) + T5(y) + T6(z)
```

`F` is `osc_3d_field_freq`. For pitched patches, values around `0.5` to `4.0` tend to stay controlled. Higher values are useful for sharp digital tones and should eventually be paired with oversampling.

## Implemented Path Examples

The active paths are:

```text
Circle        x = r cos(t), y = r sin(t), z = d sin(0.5t * zRatio)
Spiral        radius breathes while z scans a wrapped ramp
Lissajous     x:y:z ratios of 3:2:(5 * zRatio)
RandomWalk    damped noise motion, bounded by radius/depth
PhysicsOrbit  elliptical orbit with chaos-controlled eccentricity
```

`osc_3d_z_ratio` changes how quickly the z-axis evolves relative to the main phase. Integer-ish values are best for stable repeating waveforms; fractional values create longer evolving cycles.

## Coordinate Folding In The Synth

The coordinate fold is not the same as clipping the final signal. It mirrors space before the field is sampled:

```cpp
folded = foldCoordinate(p);
p = mix(p, folded, osc_3d_fold);
```

This makes the oscillator revisit mirrored regions of the same field. It is especially audible with larger `radius`, larger `depth`, high `fieldFreq`, or the `FoldedSpace` field.

## Patch Examples

Stable bell:

```text
osc_3d_field      = SphericalRipple
osc_3d_path       = Circle
osc_3d_radius     = 1.2
osc_3d_depth      = 0.7
osc_3d_field_freq = 2.5
osc_3d_z_ratio    = 1.0
osc_3d_fold       = 0.0
osc_3d_drive      = 1.2
```

Digital folded lead:

```text
osc_3d_field      = SinProduct
osc_3d_path       = Lissajous
osc_3d_radius     = 1.0
osc_3d_depth      = 1.0
osc_3d_field_freq = 4.0
osc_3d_z_ratio    = 2.0
osc_3d_warp       = 0.35
osc_3d_fold       = 0.45
osc_3d_drive      = 2.0
```

Organic drift texture:

```text
osc_3d_field      = TurbulenceNoise
osc_3d_path       = RandomWalk
osc_3d_radius     = 1.5
osc_3d_depth      = 1.2
osc_3d_field_freq = 1.5
osc_3d_chaos      = 0.30
osc_3d_warp       = 0.50
osc_3d_fold       = 0.10
osc_3d_drive      = 1.0
```

Chebyshev harmonic scan:

```text
osc_3d_field      = ChebyshevMorph
osc_3d_path       = Circle
osc_3d_radius     = 0.8
osc_3d_depth      = 0.8
osc_3d_field_freq = 1.0
osc_3d_morph      = 0.25
osc_3d_z_ratio    = 1.0
osc_3d_drive      = 1.4
```

## Modulation Examples

The modulation matrix can now target the new math controls directly:

```text
LFO2       → Osc3D_FieldFreq  amount +0.50  bipolar on
Aftertouch → Osc3D_Fold       amount +0.35  bipolar off
Aftertouch → Osc3D_Drive      amount +0.60  bipolar off
Macro1     → Osc3D_ZRatio     amount +0.75  bipolar off
```

Good musical uses:

```text
slow LFO on FieldFreq   = evolving brightness without pitch bend
velocity on Drive       = harder strikes become more saturated
aftertouch on Fold      = pressure adds digital edge
macro on ZRatio         = moves between stable and evolving 3D cycles
```

## Deferred Ideas

The guide discusses vector-field motion, attractor paths, harmonic locking, and oversampling. The current synth implements the stable core first: scalar fields, repeatable paths, z ratio, field frequency, coordinate warp/fold, morph, and drive. Full attractor-driven motion and quality-mode oversampling remain future improvements because they have a larger impact on pitch stability and CPU cost.
