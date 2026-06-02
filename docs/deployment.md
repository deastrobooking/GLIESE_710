# Deployment Guide (Developer Release Workflow)

This document defines how GLIESE 710 releases are prepared, validated, packaged,
and published for download on the project website.

Use this together with `docs/build-targets.md`:
- `build-targets.md` = how to compile and run builds.
- this guide = how to ship those builds safely.

## Scope

Release outputs for website distribution:
- macOS: `VST3`, `AU`, `CLAP`, `Standalone`
- Windows: `VST3`, `CLAP`, `Standalone`
- Linux: `VST3`, `CLAP`, `LV2`, `Standalone`

## Current Release Standard

Use this standard for GLIESE 710 and as the baseline for future ECOSYSTEM
plugin releases:

1. `build-productions/` is the canonical CMake build directory.
2. Release tags use semantic version tags in the form `vX.Y.Z`.
3. GitHub Actions is the source of truth for release bundles. Tag pushes run
   the full matrix build, tests, packaging, checksum generation, and draft
   GitHub Release creation.
4. Draft release assets are reviewed and signed before public publication.
5. Host-facing software company/vendor metadata remains `ECOSYSTEM`.
6. Release-critical dependencies are fetched from pinned archives where
   possible, not recursive git clones.
7. CPM dependency caches use platform-specific keys with restore keys so cold
   CI runs remain reliable.

## Release Checklist

1. Freeze scope for a version, for example `v0.2.0`.
2. Update plugin version in `CMakeLists.txt` with `project(... VERSION x.y.z)`.
3. Verify release metadata: `COMPANY_NAME "ECOSYSTEM"` and stable plugin IDs.
4. Run the local build/test commands in this guide, or confirm the matching PR
   CI matrix is green.
5. Merge the release-prep changes to `main`.
6. Push the annotated release tag, for example `vX.Y.Z`.
7. Wait for `.github/workflows/release.yml` to build, test, package, checksum,
   and create a draft GitHub Release.
8. Review generated assets and checksum output from the draft release.
9. Code-sign release binaries with platform-appropriate ECOSYSTEM identities.
10. Notarize and staple macOS bundles.
11. Replace draft assets with signed/notarized assets when signing is not yet
    automated in the workflow.
12. Publish the GitHub Release.
13. Upload files and metadata to the website download area.
14. Smoke-test fresh installs from website-hosted files.

## 1) Versioning and Release Metadata

Current source of truth for plugin version is in `CMakeLists.txt`:

```cmake
project(GLIESE_710 VERSION 0.1.0)
```

The host-facing software company/vendor metadata is also defined in
`CMakeLists.txt`:

```cmake
COMPANY_NAME "ECOSYSTEM"
```

Keep the company name set to `ECOSYSTEM` for release builds so DAWs and plugin
scanners list the plugin under the correct software company. The existing
reverse-DNS plugin IDs (`BUNDLE_ID`, `CLAP_ID`, and `LV2URI`) are stable plugin
identity fields; change them only as an intentional compatibility break.

For each release:
1. Bump `VERSION`.
2. Verify `COMPANY_NAME` is `ECOSYSTEM`.
3. Commit with message: `release: prepare vX.Y.Z`.
4. Tag after the release-prep PR is merged and CI is green:
   `git tag -a vX.Y.Z -m "GLIESE 710 vX.Y.Z"`.

## 2) Build + Test

From repository root:

```bash
cmake --preset production
cmake --build --preset tests --parallel
ctest --preset tests
cmake --build --preset plugin --parallel
```

Recommended targeted regression suite before shipping filter changes:

```bash
./build-productions/GLIESE_Tests_artefacts/Release/GLIESE_Tests "[filter]"
```

For a quick local rebuild before testing in a DAW or handing bundles to another
machine, use the same sequence and then verify the generated macOS bundles:

```bash
ART="build-productions/GLIESE_710_artefacts/Release"

codesign --verify --deep --strict "$ART/VST3/GLIESE 710.vst3"
codesign --verify --deep --strict "$ART/AU/GLIESE 710.component"
codesign --verify --deep --strict "$ART/CLAP/GLIESE 710.clap"
codesign --verify --deep --strict "$ART/Standalone/GLIESE 710.app"
```

If a local testing bundle reports `code has no resources but signature
indicates they must be present`, replace the generated signature with an ad-hoc
signature:

```bash
ART="build-productions/GLIESE_710_artefacts/Release"

codesign --force --deep --sign - "$ART/AU/GLIESE 710.component"
codesign --force --deep --sign - "$ART/CLAP/GLIESE 710.clap"
codesign --force --deep --sign - "$ART/Standalone/GLIESE 710.app"
```

The canonical VST3 bundle for DAW copy/install testing is:

```text
build-productions/GLIESE_710_artefacts/Release/VST3/GLIESE 710.vst3
```

If an editor or wrapper change does not appear in the copied VST3, force a clean
wrapper rebuild:

```bash
cmake --build build-productions --target GLIESE_710_VST3 --config Release --clean-first
```

On macOS, the helper script can perform the build/test pass and install AU,
VST3, and CLAP into user plugin folders with ad-hoc signing:

```bash
scripts/build-plugin.zsh --install-user
```

For VS Code-driven macOS deployment prep, use `Tasks: Run Task` and choose
`Xcode: Configure Project` to generate `build-xcode/GLIESE_710.xcodeproj`,
`Xcode: Open Project` to inspect or build in Xcode, or one of the `Deploy:*`
tasks to build, install, ad-hoc sign, and verify local plugin bundles.

## 3) Dependency Fetch and CI Runner Standard

Release builds must be reproducible on cold CI runners. Keep the dependency
fetch path simple and pinned:

- Use Node 24-ready official actions for checkout/cache steps:
  `actions/checkout@v5` and `actions/cache@v5`. Do not keep
  `actions/checkout@v4` or `actions/cache@v4` in active workflows; those are the
  Node 20 runtime lines that trigger runner deprecation warnings.
- GitHub-hosted runners satisfy the Node 24 action runner requirement. If a
  copied workflow runs on self-hosted runners, update the Actions runner before
  moving to the v5 actions.
- JUCE is fetched by CPM from the pinned JUCE release archive.
- CLAP support is fetched by CPM from pinned archives for
  `clap-juce-extensions`, CLAP core, and CLAP helpers.
- The CLAP core/helper archives are copied into the wrapper's expected
  `clap-libs/` layout during CMake configure.
- Avoid recursive dependency `git clone` paths for release-critical
  dependencies. The Windows runners have repeatedly failed there under cold
  cache conditions.
- Cache `${{ github.workspace }}/.cpm-cache` in CI, but pass a shell-normalized
  cache path to CMake with `CPM_CACHE="$(pwd -W 2>/dev/null || pwd)/.cpm-cache"`.
  This avoids raw Windows backslashes in `CPM_SOURCE_CACHE`.

Current Linux runner packages:

```bash
sudo apt-get install -y \
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
```

These packages are required before configure because JUCE builds `juceaide`
during CMake configure and needs freetype, fontconfig, GL, GTK, ALSA, and X11
development headers.

Current Windows runner hardening:

```bash
git config --global core.longpaths true
```

Keep this before dependency population to avoid path-length issues in extracted
JUCE/CLAP sources and generated Visual Studio build trees.

## 4) Collect Artefacts

Release artefacts are emitted under:

```text
build-productions/GLIESE_710_artefacts/Release/
```

Expected bundle locations:

- macOS:
  - `VST3/GLIESE 710.vst3`
  - `AU/GLIESE 710.component`
  - `CLAP/GLIESE 710.clap`
  - `Standalone/GLIESE 710.app`
- Windows:
  - `VST3/GLIESE 710.vst3`
  - `CLAP/GLIESE 710.clap`
  - `Standalone/GLIESE 710.exe`
- Linux:
  - `VST3/GLIESE 710.vst3`
  - `CLAP/GLIESE 710.clap`
  - `LV2/GLIESE 710.lv2`
  - `Standalone/GLIESE 710`

## 5) Signing and Notarization

### macOS

The helper script currently performs ad-hoc signing for local testing only. For
website releases, sign with the ECOSYSTEM `Developer ID Application` identity
and notarize.

Example signing commands:

```bash
codesign --force --deep --options runtime --sign "Developer ID Application: ECOSYSTEM (<TEAM_ID>)" "GLIESE 710.vst3"
codesign --force --deep --options runtime --sign "Developer ID Application: ECOSYSTEM (<TEAM_ID>)" "GLIESE 710.component"
codesign --force --deep --options runtime --sign "Developer ID Application: ECOSYSTEM (<TEAM_ID>)" "GLIESE 710.clap"
codesign --force --deep --options runtime --sign "Developer ID Application: ECOSYSTEM (<TEAM_ID>)" "GLIESE 710.app"
```

Then notarize a zip/pkg containing the signed bundles/app and staple tickets
before publishing.

### Windows

Sign binaries with the ECOSYSTEM Authenticode certificate before packaging.

### Linux

No standard platform gatekeeper signing requirement, but checksums are required.

## 6) Package Naming Convention

Use a deterministic naming scheme so website links are stable:

- `gliese-710-vX.Y.Z-macos-universal.zip`
- `gliese-710-vX.Y.Z-windows-x64.zip`
- `gliese-710-vX.Y.Z-linux-x64.zip`

Each zip should contain:
- plugin bundles/directories
- standalone app/executable
- `README-install.txt`
- `LICENSE.txt` (as applicable)

## 7) Stage and Package Artefacts

Create a release staging tree before zipping so the package contents are
predictable:

```text
dist/
  vX.Y.Z/
    macos/
      GLIESE 710.vst3
      GLIESE 710.component
      GLIESE 710.clap
      GLIESE 710.app
      README-install.txt
    windows/
      GLIESE 710.vst3
      GLIESE 710.clap
      GLIESE 710.exe
      README-install.txt
    linux/
      GLIESE 710.vst3
      GLIESE 710.clap
      GLIESE 710.lv2
      GLIESE 710
      README-install.txt
```

The release workflow creates this staging tree automatically from uploaded
platform artifacts. Use these commands only for a local/manual fallback after
signing/notarization:

```bash
# macOS
cd dist/vX.Y.Z && zip -r ../gliese-710-vX.Y.Z-macos-universal.zip macos

# Linux
cd dist/vX.Y.Z && zip -r ../gliese-710-vX.Y.Z-linux-x64.zip linux
```

On Windows runners, use PowerShell `Compress-Archive` for the `windows/`
staging folder.

## 8) Generate Checksums

From the release output directory:

```bash
sha256sum gliese-710-vX.Y.Z-*.zip > gliese-710-vX.Y.Z-sha256.txt
```

The release workflow generates this checksum file automatically for draft
release assets. For manual fallback releases, publish this checksum file with
the release.

## 9) Create Draft GitHub Release

Preferred automated path:

```bash
git push origin main
git push origin vX.Y.Z
```

Pushing the tag starts `.github/workflows/release.yml`. The workflow builds and
tests all supported platforms, downloads the platform artifacts, stages them
under `dist/vX.Y.Z/`, creates deterministic zip files, generates the SHA256
file, and opens a draft GitHub Release.

Use the draft release as the review checkpoint:

1. Confirm all release jobs succeeded.
2. Download each zip and verify it contains the expected platform folder and
   plugin bundles plus the Standalone app/executable.
3. Verify `gliese-710-vX.Y.Z-sha256.txt` matches the downloaded zip files.
4. Complete signing/notarization outside the workflow if signing secrets are
   not yet automated.
5. Replace unsigned draft assets with signed/notarized assets when needed.
6. Publish the draft only after the signed assets are verified.

Manual fallback if the workflow is unavailable:

```bash
gh release create vX.Y.Z \
  gliese-710-vX.Y.Z-macos-universal.zip \
  gliese-710-vX.Y.Z-windows-x64.zip \
  gliese-710-vX.Y.Z-linux-x64.zip \
  gliese-710-vX.Y.Z-sha256.txt \
  --title "GLIESE 710 vX.Y.Z" \
  --notes-file RELEASE_NOTES_vX.Y.Z.md \
  --draft
```

## 10) Release Notes Standard

Before publishing the draft, add release notes with this shape:

```md
## GLIESE 710 vX.Y.Z

### Highlights
- Main user-facing additions or improvements.

### Fixes
- Important bug fixes and compatibility fixes.

### Build And Distribution
- macOS: VST3, AU, CLAP, Standalone
- Windows: VST3, CLAP, Standalone
- Linux: VST3, CLAP, LV2, Standalone
- Standalone app included for every platform
- Vendor/software company: ECOSYSTEM
- Signing/notarization status

### Checksums
- See gliese-710-vX.Y.Z-sha256.txt.

### Known Issues
- Any known release caveats, or "None currently known."
```

Keep release notes concise and user-facing. Put engineering detail in linked
PRs or changelog entries, not in the public install notes.

## 11) Website Publishing

Use a versioned folder per release in website storage, for example:

```text
/downloads/gliese-710/vX.Y.Z/
```

Upload:
- all platform zip files
- checksum text file
- release notes markdown/html

Recommended website metadata payload:

```json
{
  "product": "GLIESE 710",
  "version": "X.Y.Z",
  "releasedAt": "YYYY-MM-DD",
  "downloads": {
    "macos": "https://<site>/downloads/gliese-710/vX.Y.Z/gliese-710-vX.Y.Z-macos-universal.zip",
    "windows": "https://<site>/downloads/gliese-710/vX.Y.Z/gliese-710-vX.Y.Z-windows-x64.zip",
    "linux": "https://<site>/downloads/gliese-710/vX.Y.Z/gliese-710-vX.Y.Z-linux-x64.zip",
    "checksums": "https://<site>/downloads/gliese-710/vX.Y.Z/gliese-710-vX.Y.Z-sha256.txt"
  }
}
```

## 12) Post-Release Validation

After publishing, verify:
1. Download links work from a clean browser session.
2. Checksums match downloaded files.
3. Plugin scan succeeds on at least one host per OS.
4. The loaded plugin reports the expected version.
5. The host/plugin scanner lists the software company/vendor as `ECOSYSTEM`.

## 13) Rollback Procedure

If a release is bad:
1. Remove/disable website links for `vX.Y.Z`.
2. Mark GitHub release as pre-release or append warning.
3. Publish a hotfix `vX.Y.(Z+1)`.

## GitHub Actions Automation

This repository includes automated release workflows:
- `.github/workflows/ci.yml`
- `.github/workflows/release.yml`

`ci.yml` runs on pushes to `main`, pull requests, and manual dispatch. It
performs a full matrix build + test on macOS, Windows, and Linux.

`release.yml` runs on tag pushes matching `v*` and:
1. Builds and tests on macOS/Windows/Linux.
2. Collects platform plugin bundles and Standalone apps/executables as
   artifacts.
3. Stages artifacts under `dist/${TAG}/<platform>/`.
4. Adds `README-install.txt` and `LICENSE.txt` to each platform package.
5. Packages deterministic zip names.
6. Generates a SHA256 checksum file.
7. Creates a draft GitHub Release with assets for that tag.

Both workflows should keep these hardening details:

1. Use `actions/checkout@v5` and `actions/cache@v5` so the workflows run on the
   Node 24 action runtime without temporary compatibility flags.
2. Cache `${{ github.workspace }}/.cpm-cache` with platform-specific keys and
   restore keys.
3. Compute `CPM_CACHE="$(pwd -W 2>/dev/null || pwd)/.cpm-cache"` in each
   configure step, then pass `-DCPM_SOURCE_CACHE="${CPM_CACHE}"` to CMake.
4. Use `build-productions` as the build directory.
5. Set `GLIESE_COPY_PLUGIN_AFTER_BUILD=OFF` in CI and release builds.
6. Use `if-no-files-found: error` for release artifact uploads.
7. Install the Linux JUCE/JUCEAIDE packages listed in this guide.
8. Run `git config --global core.longpaths true` before Windows configure.

Current release asset names:

1. `gliese-710-${TAG}-macos-universal.zip`
2. `gliese-710-${TAG}-windows-x64.zip`
3. `gliese-710-${TAG}-linux-x64.zip`
4. `gliese-710-${TAG}-sha256.txt`

The release workflow creates drafts because it does not currently embed signing
or notarization credentials. Do not publish those draft assets publicly until the
platform signing steps in this guide are complete.

### Current workflow files

- `.github/workflows/ci.yml`
- `.github/workflows/release.yml`

### Current README status badges

Add these near the repository title:

```md
[![CI](https://github.com/<owner>/<repo>/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/<owner>/<repo>/actions/workflows/ci.yml)
[![Release](https://github.com/<owner>/<repo>/actions/workflows/release.yml/badge.svg)](https://github.com/<owner>/<repo>/actions/workflows/release.yml)
```

## GitHub Actions Cost Control

Actions can become expensive when a private repository runs the full
Linux/macOS/Windows matrix repeatedly. GitHub-hosted standard runners are free
for public repositories and self-hosted runners, but private repositories use
included monthly minutes and bill overage. Windows and macOS hosted runners
consume more billable minutes than Linux, with macOS being the expensive leg of
this matrix.

Default policy:

1. Keep `ci.yml` enabled for PRs and `main` so merge-ready work gets the full
   platform signal.
2. Do local iteration first with `cmake --build --preset tests --parallel` and
   `ctest --preset tests`.
3. Push feature branches freely before opening a PR. This repo does not run CI
   for every feature branch push unless there is an open PR.
4. Open a PR when the feature needs platform validation. Expect every PR update
   to run the matrix.
5. Keep `release.yml` tag-only. Release packaging is intentionally reserved for
   `v*` tags.

For noisy feature iteration, use this ladder:

1. Prefer local builds/tests while the branch is still changing quickly.
2. If a PR is open and an intermediate commit does not need CI, include one of
   these skip markers in that commit message: `[skip ci]`, `[ci skip]`,
   `[no ci]`, `[skip actions]`, or `[actions skip]`.
3. Do not leave a merge-ready PR with only skipped required checks. Push a final
   normal commit, or manually run `ci.yml`, before merging.
4. If a workflow is broken or consuming too many minutes, temporarily disable it
   instead of deleting or renaming workflow files.

Temporary workflow shutdown:

```bash
gh workflow disable ci.yml
gh workflow enable ci.yml
```

Use the GitHub UI equivalent from `Actions -> CI -> Disable workflow` when the
CLI is unavailable. Re-enable CI before merging release-prep work. After
re-enabling, run the CI workflow manually from the Actions tab or with:

```bash
gh workflow run ci.yml
```

Avoid disabling `release.yml` except during an active billing or workflow
incident. If it must be disabled, record that in the release-prep issue/PR and
re-enable it before pushing a `v*` tag.

## Replicate This In Other Plugin Repositories (Fast Path)

Use this as a copy/paste playbook when bootstrapping another JUCE plugin repo.

### A) Copy baseline files

1. Copy `.github/workflows/ci.yml`.
2. Copy `.github/workflows/release.yml`.
3. Copy the README badges (above).
4. Copy this deployment guide section into the target repo docs.
5. Copy the CMake dependency-fetch pattern for pinned archive dependencies.

### B) Replace project-specific values

Update these values in `release.yml`:

1. Artifact source paths under `build-productions/<Product>_artefacts/Release/...`.
2. Bundle and executable names (for example `My Plugin.vst3`, `My Plugin.component`, `My Plugin.clap`, `My Plugin.lv2`, `My Plugin.app`, `My Plugin.exe`, and the Linux `My Plugin` executable).
3. Output archive names:
  - `<slug>-vX.Y.Z-macos-universal.zip`
  - `<slug>-vX.Y.Z-windows-x64.zip`
  - `<slug>-vX.Y.Z-linux-x64.zip`
  - `<slug>-vX.Y.Z-sha256.txt`

Update these values in `ci.yml` (if needed):

1. Build directory (default here is `build-productions`).
2. CMake options (`GLIESE_BUILD_TESTS`, `GLIESE_COPY_PLUGIN_AFTER_BUILD`, or equivalent project flags).
3. Required Linux packages for your plugin dependencies. JUCE plugin CI should install the freetype, fontconfig, GL, GTK, ALSA, and X11 development packages before configure so `juceaide` can build cleanly.
4. CPM cache key scope and restore keys.
5. Windows `core.longpaths` setup before configure.
6. Official action versions. Use `actions/checkout@v5` and `actions/cache@v5`
   unless a target repo is pinned to an older self-hosted runner that cannot run
   Node 24 actions yet.

For CLAP-enabled JUCE projects, prefer pinned archive fetches for `clap-juce-extensions`, CLAP core, and CLAP helpers rather than recursive submodule clones. This avoids cold-cache Windows configure failures during dependency population.

### C) Confirm workflow runtime hygiene

After copying workflows into another repo, verify that no stale Node 20 action
pins or temporary compatibility flags came along:

```bash
rg -n "actions/(checkout|cache)@v4|ACTIONS_ALLOW_USE_UNSECURE_NODE_VERSION|FORCE_JAVASCRIPT_ACTIONS_TO_NODE24" .github/workflows
```

Expected result: no matches for active workflow files. Prefer upgrading official
actions to their Node 24 runtime line over setting compatibility environment
variables.

### D) Validate in target repo

1. Open a PR and confirm all `ci.yml` jobs pass (Linux/macOS/Windows).
2. Create a throwaway tag (`v0.0.0-rc1`) and confirm release assets are published.
3. Download all generated zip files and verify checksum integrity.

### E) Keep these manual/secret-managed steps explicit

The current automated workflow intentionally does not embed signing credentials.
It creates draft releases by default for review and signing follow-up.
For production distribution, configure secure signing/notarization separately:

1. macOS: ECOSYSTEM Developer ID signing + notarization + stapling.
2. Windows: ECOSYSTEM Authenticode signing.
3. Linux: checksum verification publication.

Recommended approach:

1. Store signing secrets in GitHub Encrypted Secrets.
2. Add signing steps in `release.yml` per platform job before artifact upload.
3. Keep unsigned artifacts out of final public releases.

## Minimal Standard For New Plugin Repos

To replicate quickly with consistent quality, require this baseline:

1. `ci.yml` matrix build/test on PR and push, using Node 24-ready official
   actions for checkout/cache.
2. `release.yml` tag-triggered package + checksum + GitHub Release upload.
3. Draft releases until signing/notarization is complete.
4. Stable archive naming convention with semantic version tags (`vX.Y.Z`).
5. Pinned archive dependency fetches for release-critical third-party code.
6. CPM cache restore keys and Windows long-path setup.
7. README badges for CI and Release visibility.
8. Deployment doc including rollback and post-release verification.
