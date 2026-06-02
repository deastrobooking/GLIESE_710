#!/usr/bin/env zsh
set -euo pipefail

script_dir=${0:A:h}
repo_root=${script_dir:h}

build_dir="${repo_root}/build-productions"
config="Release"
install_scope="none"
run_tests=1

if command -v sysctl >/dev/null 2>&1; then
    jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
elif command -v nproc >/dev/null 2>&1; then
    jobs=$(nproc 2>/dev/null || echo 4)
else
    jobs=4
fi

usage() {
    cat <<'USAGE'
Usage: scripts/build-plugin.zsh [options]

Builds GLIESE 710 using the production CMake preset.

Options:
  --install-user       Copy AU/VST3/CLAP to ~/Library/Audio/Plug-Ins and sign them.
  --install-system     Copy AU/VST3/CLAP to /Library/Audio/Plug-Ins and sign them.
  --no-install         Build only. This is the default.
  --skip-tests         Build plugins without building/running tests.
  --build-dir PATH     Override build directory. Default: build-productions.
  --config NAME        Build configuration. Default: Release.
  --jobs N             Parallel build jobs.
  -h, --help           Show this help.

Examples:
  scripts/build-plugin.zsh
  scripts/build-plugin.zsh --install-user
  scripts/build-plugin.zsh --install-system
USAGE
}

die() {
    print -u2 -- "error: $*"
    exit 1
}

while (( $# > 0 )); do
    case "$1" in
        --install-user)
            install_scope="user"
            shift
            ;;
        --install-system)
            install_scope="system"
            shift
            ;;
        --no-install)
            install_scope="none"
            shift
            ;;
        --skip-tests)
            run_tests=0
            shift
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
        --jobs)
            (( $# >= 2 )) || die "--jobs needs a value"
            jobs="$2"
            shift 2
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

cmake_args=(
    -S "${repo_root}"
    -B "${build_dir}"
    -DCMAKE_BUILD_TYPE="${config}"
    -DGLIESE_BUILD_TESTS=ON
    -DGLIESE_COPY_PLUGIN_AFTER_BUILD=OFF
)

print -- "[gliese] configure: ${build_dir}"
cmake "${cmake_args[@]}"

print -- "[gliese] build: GLIESE_710_All (${config}, ${jobs} jobs)"
cmake --build "${build_dir}" --config "${config}" --target GLIESE_710_All --parallel "${jobs}"

if (( run_tests )); then
    print -- "[gliese] build: GLIESE_Tests"
    cmake --build "${build_dir}" --config "${config}" --target GLIESE_Tests --parallel "${jobs}"

    print -- "[gliese] test"
    ctest --test-dir "${build_dir}" -C "${config}" --output-on-failure
fi

bundle_binary() {
    local bundle="$1"
    local name="$2"
    print -- "${bundle}/Contents/MacOS/${name}"
}

run_or_sudo() {
    if "$@"; then
        return 0
    fi

    command -v sudo >/dev/null 2>&1 || return 1
    sudo "$@"
}

install_bundle() {
    local source_bundle="$1"
    local dest_bundle="$2"
    local dest_parent
    dest_parent=$(dirname "${dest_bundle}")

    [[ -d "${source_bundle}" ]] || die "missing built bundle: ${source_bundle}"
    case "${dest_bundle}" in
        *.vst3|*.component|*.clap)
            ;;
        *)
            die "refusing to replace unexpected bundle path: ${dest_bundle}"
            ;;
    esac

    print -- "[gliese] install: ${dest_bundle}"
    run_or_sudo mkdir -p "${dest_parent}"
    run_or_sudo rm -rf "${dest_bundle}"
    run_or_sudo ditto "${source_bundle}" "${dest_bundle}"

    if command -v codesign >/dev/null 2>&1; then
        print -- "[gliese] sign: ${dest_bundle}"
        run_or_sudo codesign --force --deep --sign - "${dest_bundle}"
        codesign --verify --deep --strict "${dest_bundle}"
    fi

    local binary
    binary=$(bundle_binary "${dest_bundle}" "GLIESE 710")
    [[ -x "${binary}" ]] || die "installed bundle is missing executable: ${binary}"
    ls -l "${binary}"
}

if [[ "${install_scope}" != "none" ]]; then
    if [[ "$(uname -s)" != "Darwin" ]]; then
        die "AU/VST3/CLAP installation automation is currently macOS-only"
    fi

    artefacts="${build_dir}/GLIESE_710_artefacts/${config}"

    if [[ "${install_scope}" == "user" ]]; then
        vst3_dir="${HOME}/Library/Audio/Plug-Ins/VST3"
        au_dir="${HOME}/Library/Audio/Plug-Ins/Components"
        clap_dir="${HOME}/Library/Audio/Plug-Ins/CLAP"
    else
        vst3_dir="/Library/Audio/Plug-Ins/VST3"
        au_dir="/Library/Audio/Plug-Ins/Components"
        clap_dir="/Library/Audio/Plug-Ins/CLAP"
    fi

    install_bundle "${artefacts}/VST3/GLIESE 710.vst3" "${vst3_dir}/GLIESE 710.vst3"
    install_bundle "${artefacts}/AU/GLIESE 710.component" "${au_dir}/GLIESE 710.component"
    install_bundle "${artefacts}/CLAP/GLIESE 710.clap" "${clap_dir}/GLIESE 710.clap"
fi

print -- "[gliese] done"
