#!/usr/bin/env bash
# Phase 7 P7.1: in-OS "gcc rebuilds binutils" oracle ISO.
#
# Reuses the gcchost ext4 (build/gcchost/ext4.img: /usr/bin/gcc + as/ar/ranlib +
# the mlibc gcc world) at RUN time. This builds the ISO: kernel + an initrd of
# the managers, the launcher-binutils driver, the full GNU userland ports
# (make/sh/sed/grep/tar/gzip/awk/cmp/diff/m4 + coreutils), and the slim
# instantos-patched binutils-2.42 source tarball. Boot headless with the gcchost
# ext4 attached and grep serial for GCCSELF_* (GCCSELF_BINUTILS_OK == P7.1 green).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/gcc-selfhost-binutils}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"
CROSS="/home/sky/gcc-port/gcc-install"
MLIBC_GCC="$BUILD_DIR/mlibc-root-gcc"
GS_DIR="$ROOT/tools/gcc-selfhost"
BU_TARBALL="$BUILD_DIR/binutils-port/binutils-insrc.tar.gz"

for t in make bash sed grep tar gzip awk cmp diff m4 libinstant.so ld-instantos.so; do
  [ -f "$BUILD_DIR/$t" ] || { echo "missing build/$t" >&2; exit 2; }
done
[ -d "$BUILD_DIR/coreutils" ] || { echo "missing build/coreutils (run tools/build-coreutils.sh)" >&2; exit 2; }
[ -f "$MLIBC_GCC/lib/libc.so" ] || { echo "gcc mlibc missing; run tools/build-mlibc-gcc.sh" >&2; exit 2; }
[ -f "$BU_TARBALL" ] || { echo "missing $BU_TARBALL (build the slim patched source)" >&2; exit 2; }
[ -f "$BUILD_DIR/gcchost/ext4.img" ] || echo "note: build/gcchost/ext4.img not found; build it with tools/gcc-port/build-gcchost-iso.sh" >&2

mkdir -p "$OUT_DIR"
if [ "${SKIP_CMAKE:-0}" != "1" ]; then
  cmake --build "$BUILD_DIR" --target INSTANTOS.EFI BOOTX64.EFI mkInitrd_proj \
        input-manager storage-manager process-manager font-manager session-manager
fi

LAUNCHER="$OUT_DIR/launcher"
"$CC" --target=x86_64-unknown-elf -ffreestanding -fPIE -fno-stack-protector \
  -nostdinc -O1 -c "$GS_DIR/launcher-binutils.c" -o "$OUT_DIR/launcher.o"
"$LD" --gc-sections --build-id=none --hash-style=sysv -z max-page-size=0x1000 \
  -pie -e _start --dynamic-linker /lib/ld-instantos.so -o "$LAUNCHER" "$OUT_DIR/launcher.o"

INITRD="$OUT_DIR/initrd.img"; ISO_ROOT="$OUT_DIR/iso"; EFI_DIR="$ISO_ROOT/EFI/BOOT"; EFI_IMG="$EFI_DIR/efiboot.img"
ISO="$OUT_DIR/gcc-selfhost-binutils.iso"
rm -rf "$ISO_ROOT"; mkdir -p "$EFI_DIR"

ENTRIES=(
  bin/input-manager:"$BUILD_DIR/input-manager"
  bin/storage-manager:"$BUILD_DIR/storage-manager"
  bin/process-manager:"$BUILD_DIR/process-manager"
  bin/font-manager:"$BUILD_DIR/font-manager"
  bin/session-manager:"$BUILD_DIR/session-manager"
  bin/login:"$LAUNCHER"
  bin/make:"$BUILD_DIR/make"
  bin/bash:"$BUILD_DIR/bash"
  bin/sh:"$BUILD_DIR/bash"
  bin/sed:"$BUILD_DIR/sed"
  bin/grep:"$BUILD_DIR/grep"
  bin/tar:"$BUILD_DIR/tar"
  bin/gzip:"$BUILD_DIR/gzip"
  bin/awk:"$BUILD_DIR/awk"
  bin/gawk:"$BUILD_DIR/awk"
  bin/cmp:"$BUILD_DIR/cmp"
  bin/diff:"$BUILD_DIR/diff"
  bin/m4:"$BUILD_DIR/m4"
  bin/binutils-insrc.tar.gz:"$BU_TARBALL"
  lib/ld-instantos.so:"$BUILD_DIR/ld-instantos.so"
  lib/libinstant.so:"$BUILD_DIR/libinstant.so"
  lib/mlibc/ld-instantos.so:"$MLIBC_GCC/lib/ld-instantos.so"
  lib/mlibc/libc.so:"$MLIBC_GCC/lib/libc.so"
  lib/mlibc/libdl.so:"$MLIBC_GCC/lib/libdl.so"
  lib/mlibc/libstdc++.so.6:"$CROSS/x86_64-unknown-instantos/lib/libstdc++.so.6"
  lib/mlibc/libgcc_s.so.1:"$CROSS/x86_64-unknown-instantos/lib/libgcc_s.so.1"
)
# coreutils: ship each produced binary as /bin/<name>.
for f in "$BUILD_DIR"/coreutils/*; do
  [ -f "$f" ] || continue
  ENTRIES+=("bin/$(basename "$f"):$f")
done

"$BUILD_DIR/mkInitrd_build/mkInitrd" "$INITRD" "${ENTRIES[@]}"

cp "$BUILD_DIR/BOOTX64.EFI" "$EFI_DIR/BOOTX64.EFI"
cp "$BUILD_DIR/INSTANTOS.EFI" "$EFI_DIR/INSTANTOS.EFI"
cp "$INITRD" "$EFI_DIR/INITRD"
dd if=/dev/zero of="$EFI_IMG" bs=1M count=96 status=none
mformat -i "$EFI_IMG" ::
mmd -i "$EFI_IMG" ::/EFI ::/EFI/BOOT
mcopy -i "$EFI_IMG" "$BUILD_DIR/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$EFI_IMG" "$BUILD_DIR/INSTANTOS.EFI" ::/EFI/BOOT/INSTANTOS.EFI
mcopy -i "$EFI_IMG" "$INITRD" ::/EFI/BOOT/INITRD
xorriso -as mkisofs -R -J -joliet-long -iso-level 3 \
  -eltorito-alt-boot -e EFI/BOOT/efiboot.img -no-emul-boot \
  -o "$ISO" "$ISO_ROOT" >/dev/null 2>&1

echo "GCCSELF_BINUTILS_ISO=$ISO"
echo "GCCSELF_DISK=$BUILD_DIR/gcchost/ext4.img"
echo "initrd entries: ${#ENTRIES[@]}"
