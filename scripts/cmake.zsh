#!/usr/bin/env zsh
set -euo pipefail

script_dir=${0:A:h}
repo_root=${script_dir:h}
portable_cmake="${repo_root}/cmake-3.31.6-macos-universal/CMake.app/Contents/bin/cmake"

if command -v cmake >/dev/null 2>&1; then
    exec cmake "$@"
fi

if [[ -x "${portable_cmake}" ]]; then
    exec "${portable_cmake}" "$@"
fi

print -u2 -- "error: cmake not found"
print -u2 -- "Install CMake 3.22+ or place the portable CMake bundle at:"
print -u2 -- "  ${portable_cmake}"
exit 127
