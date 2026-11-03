#!/bin/bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="${NEKPO_ARTIFACTS:-$root/nekpo/nekpo_artifacts}"
build="$root/build"

rm -rf "$build" "$out"
mkdir -p "$out"

cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build" -j"$(nproc)"
cmake --build "$build" --target dist

cp "$build"/dist/*.tar.gz "$out/"
cp "$build"/dist/*.deb "$out/"
cp "$build"/dist/*.AppImage "$out/"
