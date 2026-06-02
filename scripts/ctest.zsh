#!/usr/bin/env zsh
set -euo pipefail

script_dir=${0:A:h}
repo_root=${script_dir:h}
portable_ctest="${repo_root}/cmake-3.31.6-macos-universal/CMake.app/Contents/bin/ctest"

if command -v ctest >/dev/null 2>&1; then
    exec ctest "$@"
fi

if [[ -x "${portable_ctest}" ]]; then
    exec "${portable_ctest}" "$@"
fi

print -u2 -- "error: ctest not found"
print -u2 -- "Install CMake 3.22+ or place the portable CMake bundle at:"
print -u2 -- "  ${portable_ctest}"
exit 127
