# Contributing & Developer Guide

## Code Style

### Formatting
- 4-space indentation, no tabs
- `juce::` prefix always explicit (no `using namespace juce`)
- Opening braces on the same line for functions and control flow
- `{}` initializers for member variables in headers
- Line length: 100 characters soft limit

### Naming
| Element | Convention | Example |
|---|---|---|
| Classes | PascalCase | `ADSREnvelope`, `Osc3D` |
| Methods | camelCase | `processSample()`, `setParams()` |
| Members | camelCase, no prefix | `phase`, `sampleRate` |
| Parameters (APVTS) | snake_case | `osc_3d_radius`, `filter_a_cutoff` |
| Constants | `kCamelCase` | `kNumVoices`, `kTableSize` |
| Enums | PascalCase values | `FieldType::SinSumXYZ` |

### Comments
Only comment WHY, not WHAT. Avoid:
```cpp
// increment phase         ← obvious, skip it
phase += phaseInc;
```

Write:
```cpp
// Two-pass FM: seed with zero first, then use results as modulation
// to handle cross-feedback without introducing more than one sample latency
```

## Audio Thread Rules

**Never, inside `processBlock` or any method called from it:**

| Forbidden | Safe alternative |
|---|---|
| `new` / `delete` / `malloc` | Pre-allocate in `prepareToPlay` |
| `std::vector::push_back` (may realloc) | Pre-sized arrays |
| `std::mutex::lock` | `std::atomic`, APVTS params |
| File I/O | Read in message thread, copy via lock-free queue |
| System calls (`sleep`, `printf`, ...) | — |
| JUCE assertions that throw | Use `jassertfalse` (debug-only, no throw) |

Check with Address Sanitizer:
```bash
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=address"
cmake --build build
```

To detect allocations in the audio thread during testing, intercept `operator new` with a flag that asserts when set during `processBlock`. A reference implementation is in the JUCE forum thread "Allocation Detection in Audio Thread."

## Adding a Parameter

1. **Register** it in `PluginProcessor::createParameterLayout()` using `addFloat`, `addInt`, `addBool`, or `addChoice`
2. **Read** it in `SynthVoice::syncParams()` via `apvts.getRawParameterValue("id")->load()`
3. **Apply** the value to the relevant engine
4. **Wire UI**: add a `juce::Slider` or `juce::ComboBox` with a matching `SliderAttachment` or `ComboBoxAttachment` in the relevant UI panel
5. **Document**: add it to the parameter table in the relevant docs file

Parameter IDs are immutable after shipping — changing an ID breaks saved presets. Use the version number in `ParameterID{"id", version}` for breaking changes.

## Adding a New Oscillator Type

1. Create `Source/Engine/Oscillators/MyOsc.{h,cpp}`
2. Implement `prepare(double sampleRate)`, `reset()`, `setFrequency(float)`, `processSample()` returning `std::pair<float,float>`
3. Add as a member of `SynthVoice`
4. Call `prepare` in `SynthVoice::prepare()` and `reset` in `SynthVoice::reset()`
5. Add per-oscillator parameters (level, octave, ADSR at minimum)
6. Mix its output into `sigL / sigR` in `SynthVoice::renderBlock()`
7. Add the `.cpp` to `target_sources` in `CMakeLists.txt`

## Testing Philosophy

- Unit test DSP modules in isolation — no JUCE audio graph needed
- Test: envelope curve shape, oscillator output bounds, filter attenuation slope, mod matrix accumulation
- Do **not** mock the oscillators or filters — test them directly with real inputs
- Catch2 is used for its expression decomposition and section support

Add tests to [Tests/EngineTests.cpp](../Tests/EngineTests.cpp). Run with `ctest` or directly:
```bash
./build/Tests/GLIESE_Tests
```

## Debugging Tips

### Standalone is fastest
The Standalone target links everything and runs as a normal app. Use it for:
- Checking if audio runs at all
- Profiling with Instruments (macOS) or perf (Linux)
- Quick iteration before loading in a DAW

### JUCE Logger
```cpp
DBG("phase = " << phase);          // prints to Output window / stdout
juce::Logger::writeToLog("msg");   // same, works in Release
```

### Parameter inspection
```cpp
// In syncParams() — print all active mod dest values:
for (int i = 0; i < (int)ModDest::NumDests; ++i)
    if (std::abs(modDests.dests[i]) > 0.001f)
        DBG("dest[" << i << "] = " << modDests.dests[i]);
```

### Filter instability
If a filter produces NaN or Inf: add `juce::FloatVectorOperations::disableDenormalisedNumberSupport()` in `prepareToPlay`. Also add `juce::ScopedNoDenormals` at the top of `processBlock` (already present).

## Pull Request Checklist

- [ ] No allocation in audio thread (checked with sanitizer)
- [ ] New parameters registered in APVTS and read in `syncParams()`
- [ ] New parameters added to preset round-trip (covered automatically by APVTS)
- [ ] UI control wired with Attachment
- [ ] Catch2 test for any new DSP module
- [ ] Docs updated if behavior changes
- [ ] Builds on at least one platform (CI will check the rest)

## File Ownership Quick Reference

| Want to change... | Edit these files |
|---|---|
| Parameter layout | `PluginProcessor.cpp` → `createParameterLayout()` |
| 3D field types | `Osc3D.h` (enum) + `Osc3D.cpp` (evaluateField switch) |
| FM algorithms | `FMAlgorithm.cpp` (kAlgorithms table) + `FMAlgorithm.h` (kNumAlgorithms) |
| Filter modes | `FilterTypes.h` (enum) + `DualFilter.cpp` (processSample switch) |
| Mod sources/dests | `ModSource.h` / `ModDest.h` + `SynthVoice.cpp` + UI |
| Arp patterns | `Arpeggiator.h` (enum) + `Arpeggiator.cpp` (nextNote switch) |
| LFO shapes | `LFOEngine.h` (enum) + `LFOEngine.cpp` (generateShape switch) |
| UI layout | `MainPanel.cpp` → `resized()` |
| Build system | `CMakeLists.txt` |
