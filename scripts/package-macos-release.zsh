#!/usr/bin/env zsh
set -euo pipefail

script_dir=${0:A:h}
repo_root=${script_dir:h}

build_dir="${repo_root}/build-productions"
cmake_cmd="${repo_root}/scripts/cmake.zsh"
ctest_cmd="${repo_root}/scripts/ctest.zsh"
config="Release"
version=""
identity="${GLIESE_CODESIGN_IDENTITY:-}"
notary_profile="${GLIESE_NOTARY_PROFILE:-}"
team_id="${GLIESE_TEAM_ID:-}"
output_dir="${repo_root}/dist"
stage_root="${TMPDIR:-/private/tmp}/gliese-release-stage"
skip_build=0
skip_tests=0
skip_notarize=0
dry_run=0

usage() {
    cat <<'USAGE'
Usage: scripts/package-macos-release.zsh --version vX.Y.Z --identity "Developer ID Application: Name (TEAMID)" [options]

Builds, signs, stages, packages, and optionally notarizes the macOS GLIESE 710
release bundle for direct website distribution.

Options:
  --version VERSION        Release version/tag, for example v0.2.0. Required.
  --identity NAME          Developer ID Application codesigning identity.
                           Can also be set with GLIESE_CODESIGN_IDENTITY.
  --notary-profile NAME    xcrun notarytool keychain profile. If omitted,
                           notarization is skipped unless GLIESE_NOTARY_PROFILE is set.
  --team-id TEAMID         Optional Apple team ID, used only for logging/checks.
                           Can also be set with GLIESE_TEAM_ID.
  --build-dir PATH         Build directory. Default: build-productions.
  --config NAME            Build configuration. Default: Release.
  --output-dir PATH        Output directory. Default: dist.
  --stage-root PATH        Temporary staging root. Default: system temp.
  --skip-build             Use existing artefacts from the build directory.
  --skip-tests             Build plugins without building/running tests.
  --skip-notarize          Sign and zip only.
  --dry-run                Print actions without modifying files.
  -h, --help               Show this help.

Before first notarization, store Apple credentials in the login keychain:
  xcrun notarytool store-credentials "gliese-notary" --apple-id "you@example.com" --team-id "TEAMID"

Examples:
  scripts/package-macos-release.zsh --version v0.2.0 --identity "Developer ID Application: ECOSYSTEM (TEAMID)" --notary-profile gliese-notary
  GLIESE_CODESIGN_IDENTITY="Developer ID Application: ECOSYSTEM (TEAMID)" scripts/package-macos-release.zsh --version v0.2.0 --skip-notarize
USAGE
}

die() {
    print -u2 -- "error: $*"
    exit 1
}

log() {
    print -- "[gliese-release] $*"
}

run() {
    if (( dry_run )); then
        print -- "+ $*"
    else
        "$@"
    fi
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "missing required command: $1"
}

require_executable() {
    [[ -x "$1" ]] || die "missing required executable: $1"
}

while (( $# > 0 )); do
    case "$1" in
        --version)
            (( $# >= 2 )) || die "--version needs a value"
            version="$2"
            shift 2
            ;;
        --identity)
            (( $# >= 2 )) || die "--identity needs a value"
            identity="$2"
            shift 2
            ;;
        --notary-profile)
            (( $# >= 2 )) || die "--notary-profile needs a value"
            notary_profile="$2"
            shift 2
            ;;
        --team-id)
            (( $# >= 2 )) || die "--team-id needs a value"
            team_id="$2"
            shift 2
            ;;
        --build-dir)
            (( $# >= 2 )) || die "--build-dir needs a path"
            build_dir="$2"
            shift 2
            ;;
        --config)
            (( $# >= 2 )) || die "--config needs a value"
            config="$2"
            shift 2
            ;;
        --output-dir)
            (( $# >= 2 )) || die "--output-dir needs a path"
            output_dir="$2"
            shift 2
            ;;
        --stage-root)
            (( $# >= 2 )) || die "--stage-root needs a path"
            stage_root="$2"
            shift 2
            ;;
        --skip-build)
            skip_build=1
            shift
            ;;
        --skip-tests)
            skip_tests=1
            shift
            ;;
        --skip-notarize)
            skip_notarize=1
            shift
            ;;
        --dry-run)
            dry_run=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

[[ "$(uname -s)" == "Darwin" ]] || die "macOS release packaging must run on macOS"
[[ -n "${version}" ]] || die "--version is required"
[[ -n "${identity}" ]] || die "--identity is required, or set GLIESE_CODESIGN_IDENTITY"
[[ "${version}" == v* ]] || die "--version should use the release tag form vX.Y.Z"

build_dir=${build_dir:A}
output_dir=${output_dir:A}
stage_root=${stage_root:A}
artefacts="${build_dir}/GLIESE_710_artefacts/${config}"
stage_version_dir="${stage_root}/${version}"
stage_dir="${stage_version_dir}/macos"
zip_path="${output_dir}/${version}macos.zip"
checksum_path="${output_dir}/${version}sha256.txt"

bundles=(
    "${artefacts}/VST3/GLIESE 710.vst3"
    "${artefacts}/AU/GLIESE 710.component"
    "${artefacts}/CLAP/GLIESE 710.clap"
    "${artefacts}/Standalone/GLIESE 710.app"
)

require_executable "${cmake_cmd}"
require_executable "${ctest_cmd}"
require_command codesign
require_command ditto
require_command dot_clean
require_command find
require_command shasum
if command -v xattr >/dev/null 2>&1; then
    have_xattr=1
else
    have_xattr=0
fi
if (( ! skip_notarize )) && [[ -n "${notary_profile}" ]]; then
    require_command xcrun
fi

if (( ! skip_build )); then
    log "configure ${build_dir}"
    run "${cmake_cmd}" -S "${repo_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE="${config}" -DGLIESE_BUILD_TESTS=ON -DGLIESE_COPY_PLUGIN_AFTER_BUILD=OFF

    log "build GLIESE_710_All"
    run "${cmake_cmd}" --build "${build_dir}" --config "${config}" --target GLIESE_710_All --parallel

    if (( ! skip_tests )); then
        log "build GLIESE_Tests"
        run "${cmake_cmd}" --build "${build_dir}" --config "${config}" --target GLIESE_Tests --parallel

        log "run tests"
        run "${ctest_cmd}" --test-dir "${build_dir}" -C "${config}" --output-on-failure
    fi
fi

for bundle in "${bundles[@]}"; do
    (( dry_run )) || [[ -d "${bundle}" ]] || die "missing built bundle: ${bundle}"
done

log "stage ${stage_dir}"
run rm -rf "${stage_dir}"
run mkdir -p "${stage_dir}"

for bundle in "${bundles[@]}"; do
    run ditto --norsrc "${bundle}" "${stage_dir}/$(basename "${bundle}")"
done

readme_path="${stage_dir}/README-install.txt"
if (( dry_run )); then
    print -- "+ write ${readme_path}"
else
    {
        print -- "GLIESE 710 ${version} macOS installation"
        print -- ""
        print -- "Install the plugin formats you need:"
        print -- "- VST3: copy GLIESE 710.vst3 to /Library/Audio/Plug-Ins/VST3 or ~/Library/Audio/Plug-Ins/VST3"
        print -- "- AU: copy GLIESE 710.component to /Library/Audio/Plug-Ins/Components or ~/Library/Audio/Plug-Ins/Components"
        print -- "- CLAP: copy GLIESE 710.clap to /Library/Audio/Plug-Ins/CLAP or ~/Library/Audio/Plug-Ins/CLAP"
        print -- "- Standalone: move GLIESE 710.app to /Applications if desired"
        print -- ""
        print -- "Restart your DAW after copying plugins. Logic users can validate the AU with:"
        print -- "auval -v aumu GLSE G710"
    } > "${readme_path}"
fi

if [[ -f "${repo_root}/LICENSE.txt" ]]; then
    run ditto --norsrc "${repo_root}/LICENSE.txt" "${stage_dir}/LICENSE.txt"
elif [[ -f "${repo_root}/LICENSE" ]]; then
    run ditto --norsrc "${repo_root}/LICENSE" "${stage_dir}/LICENSE.txt"
fi

log "remove Finder metadata"
run dot_clean "${stage_dir}"
if (( have_xattr )); then
    run xattr -cr "${stage_dir}"
fi

sign_nested_code() {
    local bundle="$1"
    if (( dry_run )); then
        print -- "+ find ${bundle} nested code"
        return 0
    fi

    while IFS= read -r nested; do
        log "sign nested ${nested}"
        run codesign --force --timestamp --options runtime --sign "${identity}" "${nested}"
    done < <(find "${bundle}" \( -name "*.framework" -o -name "*.dylib" \) -print)
}

for staged_bundle in "${stage_dir}/GLIESE 710.vst3" "${stage_dir}/GLIESE 710.component" "${stage_dir}/GLIESE 710.clap" "${stage_dir}/GLIESE 710.app"; do
    sign_nested_code "${staged_bundle}"

    entitlements=()
    if [[ "${staged_bundle}" == *.app && -f "${repo_root}/scripts/macos-release.entitlements" ]]; then
        entitlements=(--entitlements "${repo_root}/scripts/macos-release.entitlements")
    fi

    log "sign ${staged_bundle}"
    run codesign --force --timestamp --options runtime "${entitlements[@]}" --sign "${identity}" "${staged_bundle}"

    log "verify ${staged_bundle}"
    run codesign --verify --deep --strict --verbose=2 "${staged_bundle}"
    run codesign --display --verbose=2 "${staged_bundle}"
done

log "package ${zip_path}"
run rm -f "${zip_path}"
if (( dry_run )); then
    print -- "+ (cd ${stage_version_dir} && ditto -c -k --keepParent macos ${zip_path})"
else
    mkdir -p "${output_dir}"
    (
        cd "${stage_version_dir}"
        ditto -c -k --keepParent macos "${zip_path}"
    )
fi

if (( skip_notarize )) || [[ -z "${notary_profile}" ]]; then
    log "notarization skipped"
else
    log "submit notarization using keychain profile ${notary_profile}"
    [[ -n "${team_id}" ]] && log "team ${team_id}"
    run xcrun notarytool submit "${zip_path}" --keychain-profile "${notary_profile}" --wait
fi

if (( ! dry_run )); then
    shasum -a 256 "${zip_path}" > "${checksum_path}"
    log "wrote ${checksum_path}"
fi

log "done: ${zip_path}"
