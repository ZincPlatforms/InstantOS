#!/usr/bin/env bash
# P5.5 Stage 4b: build the libinstant userland (ld-instantos, libinstant, the
# managers) with the GCC cross-compiler, by configuring outside/iUserApps as a
# standalone CMake project with cmake/instantos-userland-gcc.cmake. Outputs land
# in build/iUserApps-gcc/ (ld-instantos.so, libinstant.so, input-manager, ...).
# The clang kernel build in build/ is left untouched.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PATH="$ROOT/build/binutils-cross/bin:$PATH"

SRC="$ROOT/outside/iUserApps"
BUILD="${USERLAND_GCC_BUILD_DIR:-$ROOT/build/iUserApps-gcc}"
TOOLCHAIN="$ROOT/cmake/instantos-userland-gcc.cmake"

cmake -S "$SRC" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DINSTANTOS_KERNEL_INCLUDE="$ROOT/include" \
  -DCMAKE_BUILD_TYPE=MinSizeRel

cmake --build "$BUILD"

echo "userland-gcc built into: $BUILD"
ls -la "$BUILD"/*.so "$BUILD"/input-manager "$BUILD"/session-manager 2>/dev/null || true
