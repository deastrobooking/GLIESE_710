# Deployment Guide (Developer Release Workflow)

This document defines how GLIESE 710 releases are prepared, validated, packaged,
and published for download on the project website.

Use this together with `docs/build-targets.md`:
- `build-targets.md` = how to compile and run builds.
- this guide = how to ship those builds safely.

## Scope

Release outputs for website distribution:
- macOS: `VST3`, `AU`, `CLAP`
- Windows: `VST3`, `CLAP`
- Linux: `VST3`, `CLAP`, `LV2`

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
cmake --build --preset plugin --parallel
cmake --build --preset tests --parallel
ctest --preset tests
```

Recommended targeted regression suite before shipping filter changes:

```bash
./build-productions/GLIESE_Tests_artefacts/Release/GLIESE_Tests "[filter]"
```

## 3) Dependency Fetch and CI Runner Standard

Release builds must be reproducible on cold CI runners. Keep the dependency
fetch path simple and pinned:

- JUCE is fetched by CPM from the pinned JUCE release archive.
- CLAP support is fetched by CPM from pinned archives for
  `clap-juce-extensions`, CLAP core, and CLAP helpers.
- The CLAP core/helper archives are copied into the wrapper's expected
  `clap-libs/` layout during CMake configure.
- Avoid recursive dependency `git clone` paths for release-critical
  dependencies. The Windows runners have repeatedly failed there under cold
  cache conditions.
- Keep `CPM_SOURCE_CACHE` pointed at `${{ github.workspace }}/.cpm-cache` in CI
  and use restore keys for platform caches.

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
- Windows:
  - `VST3/GLIESE 710.vst3`
  - `CLAP/GLIESE 710.clap`
- Linux:
  - `VST3/GLIESE 710.vst3`
  - `CLAP/GLIESE 710.clap`
  - `LV2/GLIESE 710.lv2`

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
```

Then notarize a zip/pkg containing the signed bundles and staple tickets before
publishing.

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
      README-install.txt
    windows/
      GLIESE 710.vst3
      GLIESE 710.clap
      README-install.txt
    linux/
      GLIESE 710.vst3
      GLIESE 710.clap
      GLIESE 710.lv2
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
   plugin bundles.
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
- macOS: VST3, AU, CLAP
- Windows: VST3, CLAP
- Linux: VST3, CLAP, LV2
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

`ci.yml` runs on push/PR and performs a full matrix build + test on macOS,
Windows, and Linux.

`release.yml` runs on tag pushes matching `v*` and:
1. Builds and tests on macOS/Windows/Linux.
2. Collects platform bundles as artifacts.
3. Stages artifacts under `dist/${TAG}/<platform>/`.
4. Adds `README-install.txt` and `LICENSE.txt` to each platform package.
5. Packages deterministic zip names.
6. Generates a SHA256 checksum file.
7. Creates a draft GitHub Release with assets for that tag.

Both workflows should keep these hardening details:

1. Cache `${{ github.workspace }}/.cpm-cache` with platform-specific keys and
   restore keys.
2. Pass `-DCPM_SOURCE_CACHE=${{ github.workspace }}/.cpm-cache` to configure.
3. Use `build-productions` as the build directory.
4. Set `GLIESE_COPY_PLUGIN_AFTER_BUILD=OFF` in CI and release builds.
5. Use `if-no-files-found: error` for release artifact uploads.
6. Install the Linux JUCE/JUCEAIDE packages listed in this guide.
7. Run `git config --global core.longpaths true` before Windows configure.

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
2. Bundle names (for example `My Plugin.vst3`, `My Plugin.component`, `My Plugin.clap`, `My Plugin.lv2`).
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

For CLAP-enabled JUCE projects, prefer pinned archive fetches for `clap-juce-extensions`, CLAP core, and CLAP helpers rather than recursive submodule clones. This avoids cold-cache Windows configure failures during dependency population.

### C) Validate in target repo

1. Open a PR and confirm all `ci.yml` jobs pass (Linux/macOS/Windows).
2. Create a throwaway tag (`v0.0.0-rc1`) and confirm release assets are published.
3. Download all generated zip files and verify checksum integrity.

### D) Keep these manual/secret-managed steps explicit

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

1. `ci.yml` matrix build/test on PR and push.
2. `release.yml` tag-triggered package + checksum + GitHub Release upload.
3. Draft releases until signing/notarization is complete.
4. Stable archive naming convention with semantic version tags (`vX.Y.Z`).
5. Pinned archive dependency fetches for release-critical third-party code.
6. CPM cache restore keys and Windows long-path setup.
7. README badges for CI and Release visibility.
8. Deployment doc including rollback and post-release verification.
