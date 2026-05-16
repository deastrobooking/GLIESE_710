# Build Targets & CI

## CMake Targets

| Target | Description |
|---|---|
| `GLIESE_710_Standalone` | Standalone application (fastest for DSP iteration) |
| `GLIESE_710_VST3` | VST3 plugin |
| `GLIESE_710_AU` | Audio Unit (macOS only, requires Xcode) |
| `GLIESE_710_LV2` | LV2 (Linux, requires lv2 dev package) |
| `GLIESE_Tests` | Catch2 unit tests (requires `-DGLIESE_BUILD_TESTS=ON`) |

Build a single target:
```bash
cmake --build build --target GLIESE_710_Standalone --config Release
```

## Build Configurations

| Config | Use |
|---|---|
| `Debug` | Full symbols, no optimization. Enables JUCE assertions. |
| `Release` | `-O2`, no assertions. Profile with Instruments / perf. |
| `RelWithDebInfo` | `-O2` + symbols. Best for profiling with readable traces. |

## macOS — AU + VST3

```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

By default, builds stay inside the project `build/` directory. To also copy plugins into the system plugin folders, configure with:

```bash
cmake -B build -G Xcode -DGLIESE_COPY_PLUGIN_AFTER_BUILD=ON
```

When enabled, JUCE copies built plugins to:
- `/Library/Audio/Plug-Ins/VST3/GLIESE 710.vst3`
- `/Library/Audio/Plug-Ins/Components/GLIESE 710.component`

For local production builds that should stay inside the repository build directory, keep copying disabled:

```bash
cmake -S . -B build-productions -DCMAKE_BUILD_TYPE=Release -DGLIESE_BUILD_TESTS=ON -DGLIESE_COPY_PLUGIN_AFTER_BUILD=OFF
cmake --build build-productions --config Release --parallel
```

After a rebuild, restart Logic Pro or use `auval` to validate:

```bash
auval -v aumu GLSE G710
```

Expected output: `PASS` with no errors.

## Windows — VST3

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Output: `build\GLIESE_710_artefacts\Release\VST3\GLIESE 710.vst3`

Copy to `C:\Program Files\Common Files\VST3\` for DAW discovery.

## Linux — LV2 + VST3

```bash
sudo apt install lv2-dev   # Debian/Ubuntu
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Install LV2:
```bash
mkdir -p ~/.lv2
cp -r build/GLIESE_710_artefacts/Release/LV2/GLIESE\ 710.lv2 ~/.lv2/
```

Validate with:
```bash
lv2lint urn:com:gliesestudios:gliese710
```

## Running Tests

```bash
cmake -B build -DGLIESE_BUILD_TESTS=ON
cmake --build build --target GLIESE_Tests
ctest --test-dir build --output-on-failure
```

Individual test filtering:
```bash
./build/GLIESE_Tests_artefacts/GLIESE_Tests "[envelope]"
./build/GLIESE_Tests_artefacts/GLIESE_Tests "[filter]"
./build/GLIESE_Tests_artefacts/GLIESE_Tests "[osc3d]"
./build/GLIESE_Tests_artefacts/GLIESE_Tests "[terrain]"
./build/GLIESE_Tests_artefacts/GLIESE_Tests "[arp]"
```

## GitHub Actions CI (Planned — Phase 6)

Three matrix jobs:

### macOS (AU + VST3)
```yaml
runs-on: macos-14
steps:
  - uses: actions/checkout@v4
  - name: Configure
    run: cmake -B build -G Xcode -DGLIESE_BUILD_TESTS=ON
  - name: Build
    run: cmake --build build --config Release --parallel
  - name: Test
    run: ctest --test-dir build -C Release --output-on-failure
  - name: Validate AU
    run: auval -v aumu GLSE G710
  - uses: actions/upload-artifact@v4
    with:
      name: GLIESE710-macOS
      path: build/GLIESE_710_artefacts/Release/**
```

### Windows (VST3)
```yaml
runs-on: windows-latest
steps:
  - uses: actions/checkout@v4
  - name: Configure
    run: cmake -B build -G "Visual Studio 17 2022" -A x64 -DGLIESE_BUILD_TESTS=ON
  - name: Build
    run: cmake --build build --config Release --parallel
  - name: Test
    run: ctest --test-dir build -C Release --output-on-failure
  - uses: actions/upload-artifact@v4
    with:
      name: GLIESE710-Windows
      path: build\GLIESE_710_artefacts\Release\VST3\**
```

### Linux (LV2 + VST3)
```yaml
runs-on: ubuntu-24.04
steps:
  - uses: actions/checkout@v4
  - name: Install deps
    run: sudo apt-get install -y lv2-dev libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype6-dev
  - name: Configure
    run: cmake -B build -DCMAKE_BUILD_TYPE=Release -DGLIESE_BUILD_TESTS=ON
  - name: Build
    run: cmake --build build --parallel
  - name: Test
    run: ctest --test-dir build --output-on-failure
```

## CPM Cache

CPM downloads JUCE once and caches it in `~/.cache/CPM`. On CI, cache the CPM directory to avoid re-downloading on every run:

```yaml
- uses: actions/cache@v4
  with:
    path: ~/.cache/CPM
    key: ${{ runner.os }}-cpm-${{ hashFiles('CMakeLists.txt') }}
```

## Compile-Time Switches

| CMake flag | Default | Effect |
|---|---|---|
| `GLIESE_BUILD_TESTS` | OFF | Build Catch2 test executable |
| `GLIESE_COPY_PLUGIN_AFTER_BUILD` | OFF | Copy AU/VST3/LV2 artefacts into system plugin folders after build |
| `CMAKE_BUILD_TYPE` | — | Release / Debug / RelWithDebInfo |
| `JUCE_WEB_BROWSER` | 0 (off) | Prevents linking WebKit on macOS |
| `JUCE_USE_CURL` | 0 (off) | Prevents linking libcurl |
