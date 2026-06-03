# Build Targets & CI

For packaging and publishing website-ready plugin releases, follow
`docs/deployment.md`.

## CMake Targets

| Target | Description |
|---|---|
| `GLIESE_710_Standalone` | Standalone application (fastest for DSP iteration) |
| `GLIESE_710_VST3` | VST3 plugin |
| `GLIESE_710_CLAP` | CLAP plugin, built through `clap-juce-extensions` when `GLIESE_BUILD_CLAP=ON` |
| `GLIESE_710_AU` | Audio Unit (macOS only, requires Xcode) |
| `GLIESE_710_LV2` | LV2 (Linux, requires lv2 dev package) |
| `GLIESE_Tests` | Catch2 unit tests (requires `-DGLIESE_BUILD_TESTS=ON`) |

Build a single target:
```bash
cmake --build build-productions --target GLIESE_710_Standalone --config Release
```

## Build Configurations

| Config | Use |
|---|---|
| `Debug` | Full symbols, no optimization. Enables JUCE assertions. |
| `Release` | Explicit `-O3` on Clang/GCC-style compilers, `/O2` on MSVC, no assertions. Profile with Instruments / perf. |
| `RelWithDebInfo` | `-O2` + symbols. Best for profiling with readable traces. |

Release builds also pass `-fomit-frame-pointer` for AppleClang/Clang/GNU
compilers. Do not enable native CPU tuning for public universal/distribution
builds unless that compatibility tradeoff is intentional.

For local benchmarks only, configure with:

```bash
cmake -S . -B build-productions -DCMAKE_BUILD_TYPE=Release -DGLIESE_NATIVE_OPTIMIZATIONS=ON
```

That adds `-march=native` for AppleClang/Clang/GNU or `/arch:AVX2` for MSVC.
Keep it `OFF` for portable release packages.

## Plugin Company Metadata

Release builds use `COMPANY_NAME "ECOSYSTEM"` in `CMakeLists.txt`. This is the
host-facing software company/vendor name shown by DAWs and plugin scanners.
Keep the existing `BUNDLE_ID`, `CLAP_ID`, and `LV2URI` stable unless the release
is intentionally changing plugin identity.

## macOS — AU + VST3 + CLAP

Recommended local automation:

```bash
scripts/build-plugin.zsh --install-system
```

That command:
- configures `build-productions` in Release mode with tests enabled
- builds `GLIESE_710_All`
- builds and runs `GLIESE_Tests`
- copies the AU, VST3, and CLAP into `/Library/Audio/Plug-Ins`
- ad-hoc signs the installed AU/VST3/CLAP
- verifies the installed signatures

For a non-system install:

```bash
scripts/build-plugin.zsh --install-user
```

For build/test only:

```bash
scripts/build-plugin.zsh
```

The equivalent preset commands are:

```bash
cmake --preset production
cmake --build --preset plugin --parallel
cmake --build --preset tests --parallel
ctest --preset tests
```

VST3-only local build:

```bash
cmake --preset production
cmake --build --preset vst3 --parallel
```

Focused performance and safety checks:

```bash
cmake --build --preset tests --parallel
./build-productions/GLIESE_Tests_artefacts/Release/GLIESE_Tests "[performance]"
./build-productions/GLIESE_Tests_artefacts/Release/GLIESE_Tests "[filter]"
```

## VS Code + Xcode Workflow

The repository includes VS Code tasks for the normal CMake build and for an
Xcode-generated project. Open the command palette and run
`Tasks: Run Task`, then choose one of:

- `CMake: Build VST3`
- `CMake: Build All Plugins`
- `Xcode: Configure Project`
- `Xcode: Open Project`
- `Xcode: Build VST3`
- `Xcode: Build All Plugins`
- `Xcode: Run Tests`
- `Deploy: Build and Install User Plugins`
- `Deploy: Build and Install System Plugins`

The task commands call `scripts/cmake.zsh` and `scripts/ctest.zsh`, which prefer
system CMake when available and fall back to the local portable CMake bundle.

The Xcode preset generates native project files in `build-xcode/`:

```bash
scripts/cmake.zsh --preset xcode
open build-xcode/GLIESE_710.xcodeproj
```

Build from VS Code through the task list, or directly with Xcode's build tool:

```bash
xcodebuild -project build-xcode/GLIESE_710.xcodeproj -configuration Release -target GLIESE_710_VST3 -parallelizeTargets build
xcodebuild -project build-xcode/GLIESE_710.xcodeproj -configuration Release -target GLIESE_710_All -parallelizeTargets build
```

Xcode-generated bundles are emitted under:

```text
build-xcode/GLIESE_710_artefacts/Release/
```

The Xcode preset disables automatic code signing so local bundle generation does
not fail on extended metadata. Use the deploy tasks or the build script below
for ad-hoc signing and DAW install testing.

Use the `Deploy:*` VS Code tasks or `scripts/build-plugin.zsh --install-user`
when you want AU, VST3, and CLAP copied into macOS plugin folders and ad-hoc
signed for local DAW testing.

The canonical local output directory is `build-productions/`. The VST3 bundle is:

```text
build-productions/GLIESE_710_artefacts/Release/VST3/GLIESE 710.vst3
```

If the DAW shows an older editor after copying from the build folder, verify that you did not copy from stale local directories such as `build/` or `build-production/`. Force a clean VST3 wrapper rebuild with:

```bash
cmake --build build-productions --target GLIESE_710_VST3 --config Release --clean-first
```

By default, production builds stay inside `build-productions/`. To also copy plugins into the system plugin folders, configure with:

```bash
cmake -S . -B build-productions -DCMAKE_BUILD_TYPE=Release -DGLIESE_COPY_PLUGIN_AFTER_BUILD=ON
```

When enabled, JUCE copies built AU/VST3 plugins to:
- `/Library/Audio/Plug-Ins/VST3/GLIESE 710.vst3`
- `/Library/Audio/Plug-Ins/Components/GLIESE 710.component`

The CLAP wrapper performs its own user-scope copy when `GLIESE_COPY_PLUGIN_AFTER_BUILD=ON`:
- `~/Library/Audio/Plug-Ins/CLAP/GLIESE 710.clap` on macOS
- `~/.clap/GLIESE 710.clap` on Linux

For local production builds that should stay inside the repository build directory, keep copying disabled:

```bash
cmake -S . -B build-productions -DCMAKE_BUILD_TYPE=Release -DGLIESE_BUILD_TESTS=ON -DGLIESE_COPY_PLUGIN_AFTER_BUILD=OFF
cmake --build build-productions --target GLIESE_710_All --config Release --parallel
```

After a rebuild, restart Logic Pro or use `auval` to validate:

```bash
auval -v aumu GLSE G710
```

Expected output: `PASS` with no errors.

## CLAP Support

JUCE 8 does not provide a native `CLAP` entry in `juce_add_plugin(FORMATS ...)`, so GLIESE builds CLAP through `free-audio/clap-juce-extensions`. The wrapper, CLAP core, and CLAP helper sources are fetched by CPM when `GLIESE_BUILD_CLAP=ON`, which is the default.

The wrapper is pinned to a known commit in `CMakeLists.txt` because the published release tags predate the current JUCE 8 wrapper support. Its submodule dependencies are fetched as pinned archives and copied into the wrapper's expected `clap-libs/` layout during configure; this keeps CI off the more fragile recursive `git clone` path on Windows. Disable CLAP for an offline or minimal build with:

```bash
cmake -S . -B build-productions -DCMAKE_BUILD_TYPE=Release -DGLIESE_BUILD_CLAP=OFF
```

CLAP outputs are placed beside the JUCE wrapper artefacts:

```text
build-productions/GLIESE_710_artefacts/Release/CLAP/GLIESE 710.clap
```

## Windows — VST3 + CLAP

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

Outputs:
- `build\GLIESE_710_artefacts\Release\VST3\GLIESE 710.vst3`
- `build\GLIESE_710_artefacts\Release\CLAP\GLIESE 710.clap`

Copy to `C:\Program Files\Common Files\VST3\` for DAW discovery.
Copy CLAP to `C:\Program Files\Common Files\CLAP\`.

## Linux — LV2 + VST3 + CLAP

```bash
sudo apt install \
  ninja-build \
  lv2-dev \
  libasound2-dev \
  libx11-dev \
  libxrandr-dev \
  libxinerama-dev \
  libxcursor-dev \
  libxext-dev \
  libxcomposite-dev \
  libxrender-dev \
  libfreetype-dev \
  libfontconfig1-dev \
  libgl1-mesa-dev \
  libgtk-3-dev
cmake -S . -B build-productions -DCMAKE_BUILD_TYPE=Release
cmake --build build-productions --parallel
```

JUCE's CMake helper (`juceaide`) is built during configure, so Linux machines need the freetype, fontconfig, GL, GTK, ALSA, and X11 development packages before running CMake.

Install LV2:
```bash
mkdir -p ~/.lv2
cp -r build-productions/GLIESE_710_artefacts/Release/LV2/GLIESE\ 710.lv2 ~/.lv2/
```

Install CLAP:
```bash
mkdir -p ~/.clap
cp build-productions/GLIESE_710_artefacts/Release/CLAP/GLIESE\ 710.clap ~/.clap/
```

Validate with:
```bash
lv2lint urn:com:gliesestudios:gliese710
```

## Running Tests

```bash
cmake --preset production
cmake --build --preset tests --parallel
ctest --preset tests
```

Individual test filtering:
```bash
./build-productions/GLIESE_Tests_artefacts/Release/GLIESE_Tests "[envelope]"
./build-productions/GLIESE_Tests_artefacts/Release/GLIESE_Tests "[filter]"
./build-productions/GLIESE_Tests_artefacts/Release/GLIESE_Tests "[osc3d]"
./build-productions/GLIESE_Tests_artefacts/Release/GLIESE_Tests "[terrain]"
./build-productions/GLIESE_Tests_artefacts/Release/GLIESE_Tests "[arp]"
```

## GitHub Actions CI

Implemented workflows:
- `.github/workflows/ci.yml` (push/PR matrix build + test)
- `.github/workflows/release.yml` (tag-triggered build/test/package/release)

Reference matrix configuration:

Three matrix jobs:

### macOS (AU + VST3 + CLAP + Standalone)
```yaml
runs-on: macos-14
steps:
  - uses: actions/checkout@v5
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

### Windows (VST3 + CLAP + Standalone)
```yaml
runs-on: windows-latest
steps:
  - uses: actions/checkout@v5
  - name: Configure
    run: cmake -B build -G "Visual Studio 17 2022" -A x64 -DGLIESE_BUILD_TESTS=ON
  - name: Build
    run: cmake --build build --config Release --parallel
  - name: Test
    run: ctest --test-dir build -C Release --output-on-failure
  - uses: actions/upload-artifact@v4
    with:
      name: GLIESE710-Windows
      path: |
        build\GLIESE_710_artefacts\Release\VST3\**
        build\GLIESE_710_artefacts\Release\CLAP\**
        build\GLIESE_710_artefacts\Release\Standalone\**
```

### Linux (LV2 + VST3 + CLAP + Standalone)
```yaml
runs-on: ubuntu-24.04
steps:
  - uses: actions/checkout@v5
  - name: Install deps
    run: sudo apt-get install -y ninja-build lv2-dev libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libxcomposite-dev libxrender-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev libgtk-3-dev
  - name: Configure
    run: cmake -B build -DCMAKE_BUILD_TYPE=Release -DGLIESE_BUILD_TESTS=ON
  - name: Build
    run: cmake --build build --parallel
  - name: Test
    run: ctest --test-dir build --output-on-failure
  - uses: actions/upload-artifact@v4
    with:
      name: GLIESE710-Linux
      path: |
        build/GLIESE_710_artefacts/Release/VST3/**
        build/GLIESE_710_artefacts/Release/CLAP/**
        build/GLIESE_710_artefacts/Release/LV2/**
        build/GLIESE_710_artefacts/Release/Standalone/**
```

## CPM Cache

CPM downloads JUCE and CLAP dependency archives from pinned release/commit URLs and caches them in `~/.cache/CPM`. On CI, cache the CPM directory to avoid re-downloading on every run:

```yaml
- uses: actions/cache@v5
  with:
    path: ~/.cache/CPM
    key: ${{ runner.os }}-cpm-${{ hashFiles('CMakeLists.txt') }}
    restore-keys: |
      ${{ runner.os }}-cpm-
```

## Compile-Time Switches

| CMake flag | Default | Effect |
|---|---|---|
| `GLIESE_BUILD_TESTS` | OFF | Build Catch2 test executable |
| `GLIESE_BUILD_CLAP` | ON | Build the CLAP wrapper via `clap-juce-extensions` |
| `GLIESE_COPY_PLUGIN_AFTER_BUILD` | OFF | Copy plugin artefacts after build; CLAP uses the wrapper's user-scope copy path |
| `CMAKE_BUILD_TYPE` | — | Release / Debug / RelWithDebInfo |
| `JUCE_WEB_BROWSER` | 0 (off) | Prevents linking WebKit on macOS |
| `JUCE_USE_CURL` | 0 (off) | Prevents linking libcurl |
