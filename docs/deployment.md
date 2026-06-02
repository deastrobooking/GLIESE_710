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

## Release Checklist

1. Freeze scope for a version (for example `v0.2.0`).
2. Update plugin version in `CMakeLists.txt` (`project(... VERSION x.y.z)`).
3. Run tests from a clean `build-productions/`.
4. Build release artefacts for each platform.
5. Code-sign release binaries (platform-appropriate identity, not ad-hoc).
6. Notarize macOS bundles.
7. Package artefacts into platform zip files.
8. Generate checksums (`sha256`) and include them in release notes.
9. Publish Git tag + GitHub Release.
10. Upload files and metadata to the website download area.
11. Smoke-test fresh installs from website-hosted files.

## 1) Versioning

Current source of truth for plugin version is in `CMakeLists.txt`:

```cmake
project(GLIESE_710 VERSION 0.1.0)
```

For each release:
1. Bump `VERSION`.
2. Commit with message: `release: prepare vX.Y.Z`.
3. Tag after successful packaging: `git tag -a vX.Y.Z -m "GLIESE 710 vX.Y.Z"`.

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

## 3) Collect Artefacts

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

## 4) Signing and Notarization

### macOS

The helper script currently performs ad-hoc signing for local testing only. For
website releases, sign with a real `Developer ID Application` identity and
notarize.

Example signing commands:

```bash
codesign --force --deep --options runtime --sign "Developer ID Application: <TEAM>" "GLIESE 710.vst3"
codesign --force --deep --options runtime --sign "Developer ID Application: <TEAM>" "GLIESE 710.component"
codesign --force --deep --options runtime --sign "Developer ID Application: <TEAM>" "GLIESE 710.clap"
```

Then notarize a zip/pkg containing the signed bundles and staple tickets before
publishing.

### Windows

Sign binaries with Authenticode before packaging.

### Linux

No standard platform gatekeeper signing requirement, but checksums are required.

## 5) Package Naming Convention

Use a deterministic naming scheme so website links are stable:

- `gliese-710-vX.Y.Z-macos-universal.zip`
- `gliese-710-vX.Y.Z-windows-x64.zip`
- `gliese-710-vX.Y.Z-linux-x64.zip`

Each zip should contain:
- plugin bundles/directories
- `README-install.txt`
- `LICENSE.txt` (as applicable)

## 6) Stage and Package Artefacts

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

Example packaging commands (run per platform after signing/notarization):

```bash
# macOS
cd dist/vX.Y.Z && zip -r ../gliese-710-vX.Y.Z-macos-universal.zip macos

# Linux
cd dist/vX.Y.Z && zip -r ../gliese-710-vX.Y.Z-linux-x64.zip linux
```

On Windows runners, use PowerShell `Compress-Archive` for the `windows/`
staging folder.

## 7) Generate Checksums

From the release output directory:

```bash
sha256sum gliese-710-vX.Y.Z-*.zip > gliese-710-vX.Y.Z-sha256.txt
```

Publish this checksum file with the release.

## 8) Create GitHub Release

```bash
git push origin main
git push origin vX.Y.Z

gh release create vX.Y.Z \
  gliese-710-vX.Y.Z-macos-universal.zip \
  gliese-710-vX.Y.Z-windows-x64.zip \
  gliese-710-vX.Y.Z-linux-x64.zip \
  gliese-710-vX.Y.Z-sha256.txt \
  --title "GLIESE 710 vX.Y.Z" \
  --notes-file RELEASE_NOTES_vX.Y.Z.md
```

## 9) Website Publishing

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

## 10) Post-Release Validation

After publishing, verify:
1. Download links work from a clean browser session.
2. Checksums match downloaded files.
3. Plugin scan succeeds on at least one host per OS.
4. The loaded plugin reports the expected version.

## 11) Rollback Procedure

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
3. Packages deterministic zip names.
4. Generates a SHA256 checksum file.
5. Publishes assets to the GitHub Release for that tag.
