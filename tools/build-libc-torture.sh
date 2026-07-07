#!/usr/bin/env bash
# Build the libc-torture harness: a dynamically-linked mlibc program (Phase 2
# exit gate) that exercises the C library end-to-end. Mirrors build-mlibc-hello.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
MLIBC_ROOT="${MLIBC_ROOT:-$BUILD_DIR/mlibc-root}"
SRC="$ROOT/outside/iUserApps/libc-torture/src"
OUTPUT="${OUTPUT:-$BUILD_DIR/libc-torture}"
INTERPRETER="${INTERPRETER:-/lib/mlibc/ld-instantos.so}"
RPATH="${RPATH:-/lib/mlibc}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"

if [ ! -f "$MLIBC_ROOT/lib/libc.so" ]; then
  printf 'mlibc root missing libc.so: %s\nrun tools/build-mlibc.sh first\n' "$MLIBC_ROOT" >&2
  exit 2
fi

mkdir -p "$BUILD_DIR"
"$CC" --target=x86_64-unknown-elf -ffreestanding -fPIE -fno-stack-protector \
  -nostdinc -isystem "$MLIBC_ROOT/include" \
  -c "$SRC/main.c" -o "$BUILD_DIR/libc-torture.o"
"$CC" --target=x86_64-unknown-elf -ffreestanding -fPIE \
  -c "$SRC/start.S" -o "$BUILD_DIR/libc-torture-start.o"
"$LD" --gc-sections --build-id=none --hash-style=sysv -z max-page-size=0x1000 \
  -pie -e _start --dynamic-linker "$INTERPRETER" -rpath "$RPATH" \
  -o "$OUTPUT" \
  "$BUILD_DIR/libc-torture-start.o" "$BUILD_DIR/libc-torture.o" \
  -L "$MLIBC_ROOT/lib" -lc

printf 'libc-torture built: %s\n' "$OUTPUT"
