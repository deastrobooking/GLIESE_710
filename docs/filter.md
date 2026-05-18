# Dual Filter

## Architecture

GLIESE has two independent filter slots, **A** and **B**, with separate stereo state. The routing mode decides how they are combined:

```text
Series:    input -> A -> B -> output
Parallel:  input -> A --+
           input -> B --+-> average -> output
A Only:    input -> A -> output
B Only:    input -> B -> output
```

Each slot includes:

```text
cutoff
resonance
drive
morph
saturation
bass compensation
output gain
wet/dry mix
oversampling quality
filter envelope amount
LFO amount
ADSR envelope
```

## Mode List

`filter_{a,b}_type` is a choice from 31 modes:

| ID | Mode | Notes |
|---:|---|---|
| 0 | LP12 | 12 dB/oct low-pass SVF |
| 1 | LP24 | cascaded 24 dB/oct low-pass |
| 2 | HP12 | 12 dB/oct high-pass SVF |
| 3 | HP24 | cascaded 24 dB/oct high-pass |
| 4 | Bandpass | SVF band-pass |
| 5 | Notch | low-pass + high-pass blend |
| 6 | Peak | biquad peak EQ |
| 7 | LowShelf | biquad low shelf |
| 8 | HighShelf | biquad high shelf |
| 9 | Comb | feedback comb / metallic delay color |
| 10 | AM+ | additive amplitude modulation |
| 11 | AM- | multiplicative ring-style amplitude modulation |
| 12 | Ladder12 | nonlinear ladder, 12 dB slope |
| 13 | Ladder18 | nonlinear ladder, 18 dB slope |
| 14 | Ladder24 | nonlinear ladder, 24 dB slope |
| 15 | SEM Morph | Oberheim SEM-style LP -> notch -> HP morph |
| 16 | MS Dual | dirty MS-style HP into LP character |
| 17 | Formant | vowel formant resonator bank |
| 18 | Juno-106 | Roland 80017A-inspired smooth 24 dB low-pass |
| 19 | Juno-60 | IR3109-inspired warm Roland low-pass |
| 20 | Jupiter-4 | darker, driven, early Roland low-pass |
| 21 | Jupiter-8 | brighter, faster IR3109-style low-pass |
| 22 | TB-303 | 18 dB acid-style ladder with high-passed feedback character |
| 23 | P5 Rev2 SSM | Sequential SSM 2040-inspired liquid low-pass |
| 24 | P5 Rev3 CEM | Sequential CEM 3320-inspired punchy low-pass |
| 25 | Prophet-600 | darker/slower CEM 3320 variant |
| 26 | Prophet-6 | modern smooth Sequential-style low-pass |
| 27 | Pro-One | aggressive CEM monosynth low-pass |
| 28 | GLIESE Prism | original dual-SVF prism filter with split resonant color |
| 29 | Alt Ladder | alternate GLIESE ladder with native-style drive and morphable taps |
| 30 | Phys Ladder | physical modeled ladder with lossy stages, cutoff inertia, and feedback memory |

The analog-named modes are musical models, not component-perfect circuit clones. Their purpose is to give distinct response, saturation, and resonance character inside the existing GLIESE filter slot.

## Core Families

### State Variable / EQ Modes

LP12, LP24, HP12, HP24, Bandpass, Notch, Peak, LowShelf, and HighShelf use stable digital filter structures suitable for automation and modulation. They are the clean modes.
For Peak and Shelf modes, resonance narrows the emphasis and adds resonant boost even when drive is low.

### Comb And AM Modes

Comb uses a short feedback delay derived from cutoff. AM+ and AM- use the filter modulation signal as an amplitude-control source. These are sound-design utilities rather than traditional filter models.
In the AM modes, resonance acts as modulation depth plus a small self-AM color, so the resonance control remains audible even without an external AM signal.

### Ladder Modes

Ladder12, Ladder18, and Ladder24 are nonlinear low-pass models with selectable slope. Drive and saturation control how hard the signal hits the cascade; bass compensation restores low end that resonance can thin out.

Alt Ladder keeps the familiar GLIESE ladder feel but uses a different feedback tap, stronger internal gain staging, and a continuous `morph` blend from rounded 12 dB weight through 18 dB bite into the full 24 dB output. Its resonance path includes an explicit cutoff-region pole-spread tap, so resonance should sound like a clear peak rather than only a level or drive change.

Phys Ladder is the more physical modeled option. It adds per-stage cutoff spread, lossy integrator leakage, asymmetric saturation, cutoff inertia, and a memory term in the resonance path. It is a little less polite under high resonance and drive, which makes it useful before the master dual filter when an oscillator engine needs its own animated color.

### SEM Morph

The SEM mode is a 2-pole state-variable-style model. `morph` moves continuously:

```text
0.0 = low-pass
0.5 = notch
1.0 = high-pass
```

This is the Oberheim-inspired airy/sizzly filter slot.

### Roland Character Modes

The Roland family gives four flavors:

```text
Juno-106   smoother, slightly compressed
Juno-60    warmer, creamier IR3109 response
Jupiter-4  darker and more driven
Jupiter-8  brighter and faster
```

All four are 24 dB/oct low-pass character models. `drive`, `saturation`, and `morph` influence how strongly the input and feedback paths color the sound.
`resonance` controls both the feedback amount and a resonant stage tap, so the cutoff peak stays obvious across the vintage Roland variants.

The Juno models now use separate OTA-card calibrations for the 80017A/Juno-106 and IR3109/Juno-60 behaviors. The Jupiter models add a more physical voice-card response: input coupling, lossy stage leakage, skewed feedback taps, and per-stage cutoff spread. Jupiter-4 is darker and more driven; Jupiter-8 is faster, cleaner, and brighter.

### TB-303

The TB-303 mode is an acid-style 18 dB/oct character filter. Its resonance path is shaped differently from a standard four-pole ladder, giving the more nasal, liquid, less heavy peak associated with 303-style sweeps.
High resonance adds a stronger squelchy tap around the cutoff while preserving the high-passed feedback character.
`morph` behaves like an accent/sweep macro: higher values push the cutoff, shorten the internal slew, add self-FM from the previous output, and make the diode feedback path bite harder.

Best with:

```text
high resonance
moderate drive
positive filter envelope amount
short decay
accent-like velocity or macro modulation
```

### Sequential Character Modes

The Sequential modes cover the requested SSM/CEM-inspired palette:

```text
P5 Rev2 SSM     warm, liquid, smooth resonance
P5 Rev3 CEM     brighter, punchier, stronger resonance
Prophet-600     darker and slower
Prophet-6       cleaner modern response
Pro-One         aggressive monosynth bite
```

`morph` is available as a mode-dependent color control. In Prophet-6 style patches it can be used like a modern slope/color control; in other Sequential modes it acts as extra character shaping.

### GLIESE Prism

GLIESE Prism is an original filter, not a clone. It runs two resonant TPT state-variable cores at split cutoff frequencies, cross-excites their band outputs, and folds the result through a soft energy limiter. `morph` widens the split and moves the sound from rounded low-pass mass toward glassy, animated resonant color. `bass_comp` restores body under high resonance.

## Engine Oscillator Filters

Each oscillator engine can now run through its own per-voice stereo filter before the master dual filter:

```text
3D      -> Engine Filter 3D      \
FM      -> Engine Filter FM       \
WT      -> Engine Filter WT        \
VA      -> Engine Filter VA         -> voice sum -> Filter A/B
KS      -> Engine Filter KS        /
GRAN    -> Engine Filter GRAN     /
TERRAIN -> Engine Filter TERRAIN /
```

The engine filters are mixed into the voice before Filter A/B. They use the same filter model list as the master dual filter, so the native filters, the analog character models, Alt Ladder, Phys Ladder, and GLIESE Prism can all be used per oscillator family.

Each engine filter has:

```text
on
type
cutoff
resonance
drive
morph
saturation
bass compensation
wet/dry mix
oversampling quality
engine envelope amount
engine LFO amount
```

The editor exposes these on the **ENGINE FILTERS** page. They are intended for source-level shaping: for example, filtering FM harshness before it hits the master filter, putting a Phys Ladder only on the VA stack, or giving granular/terrain layers their own darker pre-filter.

## Cutoff Modulation

Cutoff modulation is applied in octaves:

```text
cutoffHz = baseCutoffHz * 2^(envAmount * 4 * envValue + lfoAmount * 2 * lfoValue)
```

- Filter envelope depth reaches up to about +/-4 octaves.
- Filter LFO depth reaches up to about +/-2 octaves.
- Result is clamped to the safe audio range.

This keeps sweeps musical across the full range.

## Parameters

For slot `s` (`a` or `b`):

| Parameter | Range | Description |
|---|---|---|
| `filter_{s}_type` | 0-30 | Filter mode. |
| `filter_{s}_cutoff` | 20-20000 Hz | Base cutoff. |
| `filter_{s}_resonance` | 0-1 | Resonance / Q / feedback. |
| `filter_{s}_drive` | 0-1 | Pre-filter input drive. |
| `filter_{s}_morph` | 0-1 | Mode-dependent morph/color. |
| `filter_{s}_saturation` | 0-1 | Nonlinear shaping amount. |
| `filter_{s}_bass_comp` | 0-1 | Bass compensation for nonlinear low-pass models. |
| `filter_{s}_gain` | -24 to +24 dB | Output trim. |
| `filter_{s}_mix` | 0-1 | Wet/dry blend. |
| `filter_{s}_oversample` | Off / 2x / 4x | Quality mode for nonlinear models. |
| `filter_{s}_env_amt` | -1 to +1 | Filter envelope depth. |
| `filter_{s}_lfo_amt` | -1 to +1 | LFO depth. |
| `filter_{s}_adsr_a/d/s/r` | envelope | Per-filter envelope. |

Global:

| Parameter | Range | Description |
|---|---|---|
| `filter_routing` | 0-3 | Series / Parallel / A Only / B Only. |

Per engine filter, where `e` is `3d`, `fm`, `wt`, `va`, `ks`, `gran`, or `terrain`:

| Parameter | Range | Description |
|---|---|---|
| `engine_filter_{e}_on` | off/on | Enable the oscillator-family filter. |
| `engine_filter_{e}_type` | 0-30 | Filter mode. |
| `engine_filter_{e}_cutoff` | 20-20000 Hz | Base cutoff. |
| `engine_filter_{e}_resonance` | 0-1 | Resonance / Q / feedback. |
| `engine_filter_{e}_drive` | 0-1 | Pre-filter input drive. |
| `engine_filter_{e}_morph` | 0-1 | Mode-dependent morph/color. |
| `engine_filter_{e}_saturation` | 0-1 | Nonlinear shaping amount. |
| `engine_filter_{e}_bass_comp` | 0-1 | Bass compensation for nonlinear low-pass models. |
| `engine_filter_{e}_mix` | 0-1 | Wet/dry blend. |
| `engine_filter_{e}_oversample` | Off / 2x / 4x | Quality mode for nonlinear models. |
| `engine_filter_{e}_env_amt` | -1 to +1 | Engine-envelope cutoff depth. |
| `engine_filter_{e}_lfo_amt` | -1 to +1 | Engine LFO cutoff depth. |

## Adding A New Filter Mode

1. Add an entry to `FilterMode` in [FilterTypes.h](../Source/Engine/Filter/FilterTypes.h).
2. Add or reuse a model in [FilterModels.h](../Source/Engine/Filter/FilterModels.h).
3. Route it in [DualFilter.cpp](../Source/Engine/Filter/DualFilter.cpp).
4. Add the display name to the APVTS choice list in `PluginProcessor.cpp`.
5. Add the same display name to `FilterPanel.cpp`.
6. Add a focused audio test in [EngineTests.cpp](../Tests/EngineTests.cpp).
