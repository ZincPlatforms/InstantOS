#!/usr/bin/env bash
# Build and run the host-side ACPI fixture tests.
#
# These tests exercise the shared ACPI table-parsing logic
# (include/cpu/acpi/acpi_tables.hpp) against synthetic ACPI 1.0/2.0/6.0
# firmware images. They run entirely on the host - no QEMU, OVMF, or kernel
# build required - so they are cheap enough to run in CI on every change.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/acpi-fixtures}"
CXX="${CXX:-clang++}"
SRC="$ROOT/tools/acpi-fixtures/acpi_fixture_tests.cpp"
BIN="$BUILD_DIR/acpi_fixture_tests"

if ! command -v "$CXX" >/dev/null 2>&1; then
  printf 'error: host C++ compiler "%s" not found (set CXX=...)\n' "$CXX" >&2
  exit 2
fi

mkdir -p "$BUILD_DIR"

printf 'building ACPI fixture tests with %s\n' "$CXX"
"$CXX" -std=c++23 -O2 -Wall -Wextra -Wno-address-of-packed-member \
  -I"$ROOT/include" \
  "$SRC" -o "$BIN"

printf 'running ACPI fixture tests\n'
"$BIN"
