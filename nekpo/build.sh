#!/bin/bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${NEKPO_ARTIFACTS:-$root/nekpo/nekpo_artifacts}"
build_vita="$root/build-vita"
build_desktop="$root/build-desktop"

mkdir -p "$out"

cmake -S "$root" -B "$build_vita" -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
cmake -S "$root" -B "$build_desktop" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_vita" -j"$(nproc)"
cmake --build "$build_desktop" -j"$(nproc)"
cmake --build "$build_desktop" --target dist -j"${nproc}"

cp "$build_vita"/*.vpk "$out/"
cp "$build_desktop"/dist/*.tar.gz "$out/"
cp "$build_desktop"/dist/*.deb "$out/"
cp "$build_desktop"/dist/*.AppImage "$out/"
