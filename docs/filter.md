# Dual Filter

## Architecture

Two independent filter slots (**A** and **B**) with selectable routing. Each slot keeps separate left/right DSP state, so stereo input is filtered without cross-feeding channel history.

```
Input
  │
  ├── Series:   Input → Filter A → Filter B → Output
  │
  ├── Parallel: Input → Filter A ─╮
  │             Input → Filter B ─┴→ (A+B)/2 → Output
  │
  ├── A Only:   Input → Filter A → Output
  │
  └── B Only:   Input → Filter B → Output
```

Each filter has its own cutoff, resonance, drive, filter envelope, and LFO modulation.

## Filter Modes

### State Variable (LP12, LP24, HP12, HP24, Bandpass)

Uses `juce::dsp::StateVariableTPTFilter` (Topology-Preserving Transform). This is the best choice for audio-rate modulation because the TPT structure avoids the numerical instability of direct-form biquads at high cutoff/resonance combinations.

- **LP12 / HP12**: single 12dB/octave SVF stage
- **LP24 / HP24**: two cascaded 12dB SVF stages (24dB/octave)
- **Bandpass**: single SVF in bandpass mode

### Notch

Sum of LP and HP outputs from two parallel SVFs:
```
output = (LP_output + HP_output) × 0.5
```

### Peak / LowShelf / HighShelf

Uses `juce::dsp::IIR::Coefficients` (biquad) for EQ-style filtering. The `drive` knob sets the shelf/peak gain in dB.

### Comb

Feedback comb filter using a delay line:
```
output   = input + feedback × delayed
delay_ms = sampleRate / cutoff_Hz
feedback = resonance × 0.5
```
Produces metallic, flanger-like, and physical-modeling-adjacent tones.

### AM+ (Amplitude Modulation, additive)

```
output = input × (1 + amMod × envAmount)
```
Tremolo when `amMod` is a slow LFO. Ring modulation-like when `amMod` is a fast oscillator signal.

### AM- (Amplitude Modulation, multiplicative)

```
output = input × amMod
```
True ring modulation. With `amMod` from a sine LFO: sum and difference sidebands.

## Cutoff Modulation

Cutoff is modulated in **octaves** from the base frequency:

```
cutoffHz = baseCutoffHz × 2^(envAmount × 4 × envValue + lfoAmount × 2 × lfoValue)
```

- Envelope can add or subtract up to ±4 octaves
- LFO can add or subtract up to ±2 octaves
- Result is clamped to [20 Hz, Nyquist]

This log-domain modulation means the filter tracks evenly across the audible range (each semitone is the same ratio, not the same Hz offset).

## Per-Filter ADSR

Each filter has a dedicated `ADSREnvelope`. Its output (0–1) is passed to `SingleFilter::processSample()` as `envValue`. The `filter_{a,b}_env_amt` parameter (-1..+1) scales whether the envelope opens or closes the filter.

Typical patch:
```
filter_a_cutoff   = 400 Hz     (dark baseline)
filter_a_env_amt  = +0.8       (envelope opens it)
filter_a_adsr_a   = 0.02s      (fast attack)
filter_a_adsr_d   = 0.4s       (decay into sustain)
filter_a_adsr_s   = 0.2        (partial sustain level)
filter_a_adsr_r   = 0.6s       (smooth release)
```
Result: bright initial pluck, settles to a darker sustained tone.

## Drive

The `drive` parameter (0–1) applies a pre-filter saturation:

```cpp
driven = tanh(input × (1 + drive × 4))
```

At `drive = 0`: clean pass-through.  
At `drive = 1`: input amplified 5× before filter, heavily saturated.

Drive before the filter, not after — this means resonance at high drive produces fat, harmonically rich self-oscillation rather than harsh clipping.

## Parameters

### Shared per filter (slot = `a` or `b`)

| Parameter | Range | Description |
|---|---|---|
| `filter_{s}_type` | 0–11 | Mode (see list above) |
| `filter_{s}_cutoff` | 20–20000 Hz | Base cutoff |
| `filter_{s}_resonance` | 0–1.0 | Resonance / Q |
| `filter_{s}_drive` | 0–1.0 | Pre-filter saturation |
| `filter_{s}_env_amt` | −1 to +1 | Envelope depth (octaves) |
| `filter_{s}_lfo_amt` | −1 to +1 | LFO depth (octaves) |
| `filter_{s}_adsr_a` | 0.001–60s | Env attack |
| `filter_{s}_adsr_d` | 0.001–60s | Env decay |
| `filter_{s}_adsr_s` | 0–1.0 | Env sustain level |
| `filter_{s}_adsr_r` | 0.001–60s | Env release |

### Global
| Parameter | Range | Description |
|---|---|---|
| `filter_routing` | 0–3 | Series / Parallel / A Only / B Only |

## Adding a New Filter Mode

1. Add entry to `FilterMode` enum in [FilterTypes.h](../Source/Engine/Filter/FilterTypes.h)
2. Add a `case` to `SingleFilter::processSample()` in [DualFilter.cpp](../Source/Engine/Filter/DualFilter.cpp)
3. Add the mode name to both the APVTS choice parameter in `PluginProcessor.cpp` and the `filterModes` list in `FilterPanel.cpp`
