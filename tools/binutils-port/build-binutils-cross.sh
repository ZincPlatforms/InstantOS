#!/usr/bin/env bash
# P4 Step A: cross-build GNU binutils on the host (WSL) targeting InstantOS,
# producing x86_64-unknown-instantos-{as,ld,ar,ranlib,nm,strip,...} in
# build/binutils-cross/bin. Idempotent: skips download/extract/build steps that
# are already done. Re-run after editing apply-instantos-port.sh.
#
# Requires host toolchain + texinfo-free build (MAKEINFO=true skips docs).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
PORT_DIR="$BUILD_DIR/binutils-port"
VER="${VER:-binutils-2.42}"
URL="${URL:-https://ftp.gnu.org/gnu/binutils/${VER}.tar.xz}"
SRC="$PORT_DIR/$VER"
OBJ="$PORT_DIR/build-cross"
PREFIX="${PREFIX:-$BUILD_DIR/binutils-cross}"
MLIBC_ROOT="${MLIBC_ROOT:-$BUILD_DIR/mlibc-root}"
TARGET="x86_64-unknown-instantos"
JOBS="${JOBS:-4}"

mkdir -p "$PORT_DIR"

# 1. fetch + extract (download to fast native /tmp first; /mnt/c writes are slow)
if [ ! -d "$SRC" ]; then
  if [ ! -f "$PORT_DIR/$VER.tar.xz" ]; then
    echo "downloading $URL"
    curl -sSL -m 300 "$URL" -o /tmp/$VER.tar.xz
    cp /tmp/$VER.tar.xz "$PORT_DIR/$VER.tar.xz"
  fi
  echo "extracting $VER"
  tar -C "$PORT_DIR" -xf "$PORT_DIR/$VER.tar.xz"
fi

# 2. apply the x86_64-unknown-instantos port (idempotent)
bash "$ROOT/tools/binutils-port/apply-instantos-port.sh" "$SRC"

# 3. configure (target=instantos, mlibc sysroot, SysV hash default, no NLS/docs)
if [ ! -f "$OBJ/Makefile" ]; then
  rm -rf "$OBJ"; mkdir -p "$OBJ"
  ( cd "$OBJ" && "$SRC/configure" \
      --target="$TARGET" \
      --prefix="$PREFIX" \
      --with-sysroot="$MLIBC_ROOT" \
      --disable-nls --disable-werror \
      --enable-default-hash-style=sysv )
fi

# 4. build + install (MAKEINFO=true: no texinfo needed)
make -C "$OBJ" -j"$JOBS" MAKEINFO=true
make -C "$OBJ" MAKEINFO=true install

echo "binutils-cross installed to $PREFIX"
ls "$PREFIX/bin"
