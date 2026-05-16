# Karplus-Strong Oscillator

## Concept

Karplus-Strong synthesis models a plucked string by:

1. Filling a short delay buffer with an excitation signal (noise or a glitch burst)
2. Feeding the buffer output back through a lowpass filter and into its own input
3. Repeating — the loop period matches the desired note pitch

The loop acts like a resonating string: the LPF removes high frequencies each cycle (simulating damping), and the delay length sets the pitch by controlling how many samples fit in one period.

## Signal Flow

```
noteOn trigger
    │
    ▼
pluck() — fill delay buffer with excitation signal
    │       (noise + glitch blend, comb-filtered by pluck position)
    │
    ▼
processSample() [called each audio sample]
    │
    ├── readInterpolated(periodSamples − 0.5)   ← fractional-delay read
    │
    ├── APF stiffness filter                    ← first-order all-pass
    │     output = −g·input + lastAPFIn + g·lastAPFOut
    │
    ├── LPF brightness filter                   ← weighted average
    │     output = input × brightness + lastLPFOut × (1 − brightness)
    │
    ├── decay scale                             ← output × decay
    │
    └── write back to delay buffer → output
```

## Excitation

`pluck()` fills exactly one period of the delay buffer with a blend of two excitation signals:

```
excitation = noise + morph × (glitch − noise)
```

- **Noise (`morph = 0`)**: white noise — produces a natural plucked-string attack
- **Glitch (`morph = 1`)**: a quantized square burst (`i % 4 > 1 ? +1 : −1`) — produces a harsh, digital pluck

A comb filter based on `position` is applied during fill:

```cpp
excitation -= 0.5 × delayBuffer[writeIdx − combDelay]
```

This introduces a notch at harmonics related to the pluck point. Lower `position` (plucked near the bridge) emphasizes higher harmonics and produces a brighter tone.

## Stiffness All-Pass Filter

A first-order all-pass filter adds frequency-dependent dispersion, simulating string stiffness:

```
output = −g × input + lastAPFIn + g × lastAPFOut
```

At `stiffness = 0`, the APF is bypassed (g = 0, identity). As `stiffness` increases, high frequencies are increasingly phase-shifted, causing inharmonic stretching — the same effect that makes piano strings sharp in the upper register.

## LPF Brightness

A one-pole IIR lowpass controls loop damping:

```
output = input × brightness + lastLPFOut × (1 − brightness)
```

- `brightness = 1.0` — no filtering, sustained bright tone
- `brightness = 0.0` — strong filtering, fast decay to a dull thud

The LPF is the primary timbre control. At medium values (0.3–0.7) the string decays naturally with high-frequency content fading first, matching real string behavior.

## Decay

After the LPF, the output is scaled by `decay` before being written back:

```
loopOutput = lpfOutput × decay
```

`decay` maps from the APVTS `osc_ks_decay` param (stored as 0–1) to the feedback range 0.99–0.9999:

```cpp
cachedKSParams.decay = 0.99f + 0.0099f × decayParam;
```

At the minimum (0.99), the string decays in roughly 100 cycles ≈ 0.2 s at 440 Hz. At the maximum (0.9999), it sustains for thousands of cycles — useful for very long, piano-like or harp-like tails.

## Pitch Accuracy

Fractional delay interpolation provides sub-sample pitch accuracy:

```cpp
float readDelay = periodSamples − 0.5;
int   n    = floor(readDelay);
float frac = readDelay − n;
out  = buffer[readIdx] × (1 − frac) + buffer[readIdx − 1] × frac;
```

The `− 0.5` offset compensates for the half-sample group delay introduced by the LPF, keeping the overall loop tuned to the correct pitch.

## Envelope

KS has its own amplitude envelope (`ENV_KS`) which gates the output independently of the string's internal decay. The string's natural decay and the ADSR envelope interact multiplicatively.

For realistic plucked behavior, set the ADSR to:
```
Attack  = 0.001 s (instant)
Decay   = 0.1 s
Sustain = 0.0
Release = 1.0 s
```

This lets the KS loop decay freely after the note-off release phase.

## Parameters

| Parameter | APVTS ID | Range | Effect |
|---|---|---|---|
| Enable | `osc_ks_on` | on/off | Hard-bypasses the oscillator |
| Level | `osc_ks_level` | 0–1 | Output amplitude |
| Octave | `osc_ks_octave` | −3–+3 | Octave transpose |
| Decay | `osc_ks_decay` | 0–1 | Loop feedback: 0 = 0.990 (fast), 1 = 0.9999 (slow) |
| Brightness | `osc_ks_brightness` | 0–1 | LPF blend: 1 = bright/slow-decay, 0 = dark/fast-decay |
| Stiffness | `osc_ks_stiffness` | 0–0.95 | APF all-pass coefficient — higher = more inharmonic stretch |
| Morph | `osc_ks_morph` | 0–1 | Excitation blend: 0 = white noise, 1 = glitch burst |
| Position | `osc_ks_position` | 0.05–0.95 | Pluck position: 0 = bridge (bright), 1 = middle (dark) |
| Env Attack | `osc_ks_adsr_a` | 0.001–60 s | Amplitude envelope |
| Env Decay | `osc_ks_adsr_d` | 0.001–60 s | |
| Env Sustain | `osc_ks_adsr_s` | 0–1 | |
| Env Release | `osc_ks_adsr_r` | 0.001–60 s | |

## Patch Examples

**Natural nylon guitar**
```
decay      = 0.70,  brightness = 0.55
stiffness  = 0.00,  morph      = 0.05
position   = 0.30
ADSR: A=0.001, D=0.1, S=0, R=1.5
```

**Steel-string acoustic**
```
decay      = 0.78,  brightness = 0.65
stiffness  = 0.05,  morph      = 0.0
position   = 0.25
ADSR: A=0.001, D=0.1, S=0, R=2.0
```

**Hammered dulcimer / piano**
```
decay      = 0.88,  brightness = 0.72
stiffness  = 0.15,  morph      = 0.0
position   = 0.20
ADSR: A=0.001, D=0.2, S=0, R=3.0
```

**Electric glitch bass**
```
decay      = 0.80,  brightness = 0.45
stiffness  = 0.30,  morph      = 0.60
position   = 0.35,  octave     = −1
ADSR: A=0.001, D=0.05, S=0, R=0.8
```

**Percussive click body (combined with 3D or FM)**
```
decay      = 0.55,  brightness = 0.30
stiffness  = 0.00,  morph      = 0.80
position   = 0.50,  level      = 0.4
ADSR: A=0.001, D=0.05, S=0, R=0.3
```

## Implementation Notes

- **Buffer size**: `kMaxDelaySamples = 8192`. At 48 kHz this supports pitches down to ~5.9 Hz (below MIDI note 0), so the buffer is never a limiting factor for musical use.
- **Active flag**: `isActive()` returns false once the loop output drops below `1e-6`. The voice render loop checks this and skips further KS processing, reclaiming CPU.
- **Pluck on every noteOn**: `ksOsc.pluck()` is called in `SynthVoice::noteOn()` using `cachedKSParams` (the params from the previous render block). This means the first note of a session uses default KS params — subsequent notes use the live param values.
- **No retrigger guard**: playing a new note while the previous string is still ringing re-excites the same delay buffer. This is intentional — it simulates re-plucking a still-vibrating string. To avoid this, set short decay or use voice steal.

## Source Files

- [KarplusStrong.h](../Source/Engine/Oscillators/KarplusStrong.h)
- [KarplusStrong.cpp](../Source/Engine/Oscillators/KarplusStrong.cpp)
