#!/usr/bin/env bash
# Build a headless InstantOS ISO whose /bin/login is the bash-fork regression
# driver (tools/tcc-selfhost/bashfork.c). It spawns /bin/bash repeatedly and runs
# /bin/forktest.sh, which hammers bash's fork/exec paths -- the ones that used to
# int3-storm the OS. Boot under QEMU and grep serial for BASHFORK_* markers.
#
# Env: SKIP_CMAKE=1 to reuse the already-built kernel + mkInitrd (default builds).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/bashfork}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"
CC_TARGET_FLAG="${CC_TARGET_FLAG---target=x86_64-unknown-elf}"
LAUNCHER_EXTRA_CFLAGS="${LAUNCHER_EXTRA_CFLAGS:-}"
SKIP_CMAKE="${SKIP_CMAKE:-0}"

MLIBC_ROOT="${MLIBC_ROOT:-$BUILD_DIR/mlibc-root}"
BASH_BIN="${BASH_BIN:-$BUILD_DIR/bash}"
TCC_BIN="${TCC_BIN:-$BUILD_DIR/tcc}"
# Runtime substrate (managers + ld-instantos.so + libinstant.so). Defaults to the
# clang cmake build; the GCC world overrides SUBSTRATE_DIR + LIBINSTANT_SO.
SUBSTRATE_DIR="${SUBSTRATE_DIR:-$BUILD_DIR}"
LIBINSTANT_SO="${LIBINSTANT_SO:-$BUILD_DIR/libinstant.so}"
SELFHOST_DIR="$ROOT/tools/tcc-selfhost"

[ -f "$BASH_BIN" ]                || { echo "bash missing; run tools/build-bash.sh" >&2; exit 2; }
[ -f "$MLIBC_ROOT/lib/libc.so" ]  || { echo "mlibc missing; run tools/build-mlibc.sh" >&2; exit 2; }
[ -f "$TCC_BIN" ]                 || { echo "tcc missing; cmake --build build --target tcc" >&2; exit 2; }

mkdir -p "$OUT_DIR"
if [ "$SKIP_CMAKE" != "1" ]; then
  cmake --build "$BUILD_DIR" --target INSTANTOS.EFI BOOTX64.EFI mkInitrd_proj
fi

# Bash-fork driver becomes /bin/login.
LAUNCHER="$OUT_DIR/bashfork"
"$CC" $CC_TARGET_FLAG -ffreestanding -fPIE -fno-stack-protector \
  -nostdinc $LAUNCHER_EXTRA_CFLAGS -c "$SELFHOST_DIR/bashfork.c" -o "$OUT_DIR/bashfork.o"
"$LD" --gc-sections --build-id=none --hash-style=sysv -z max-page-size=0x1000 \
  -pie -e _start --dynamic-linker /lib/ld-instantos.so \
  -o "$LAUNCHER" "$OUT_DIR/bashfork.o"

INITRD="$OUT_DIR/initrd.img"
ISO_ROOT="$OUT_DIR/iso"
EFI_DIR="$ISO_ROOT/EFI/BOOT"
EFI_IMG="$EFI_DIR/efiboot.img"
ISO="$OUT_DIR/bashfork.iso"

rm -rf "$ISO_ROOT"; mkdir -p "$EFI_DIR"

# managers + bashfork driver as /bin/login + bash + tcc + forktest.sh + runtime.
ENTRIES=(
  bin/input-manager:"$SUBSTRATE_DIR/input-manager"
  bin/storage-manager:"$SUBSTRATE_DIR/storage-manager"
  bin/process-manager:"$SUBSTRATE_DIR/process-manager"
  bin/font-manager:"$SUBSTRATE_DIR/font-manager"
  bin/session-manager:"$SUBSTRATE_DIR/session-manager"
  bin/login:"$LAUNCHER"
  bin/bash:"$BASH_BIN"
  bin/tcc:"$TCC_BIN"
  bin/forktest.sh:"$SELFHOST_DIR/forktest.sh"
  lib/ld-instantos.so:"$SUBSTRATE_DIR/ld-instantos.so"
  lib/libinstant.so:"$LIBINSTANT_SO"
  lib/mlibc/ld-instantos.so:"$MLIBC_ROOT/lib/ld-instantos.so"
  lib/mlibc/libc.so:"$MLIBC_ROOT/lib/libc.so"
  lib/mlibc/libdl.so:"$MLIBC_ROOT/lib/libdl.so"
)

"$BUILD_DIR/mkInitrd_build/mkInitrd" "$INITRD" "${ENTRIES[@]}"

cp "$BUILD_DIR/BOOTX64.EFI" "$EFI_DIR/BOOTX64.EFI"
cp "$BUILD_DIR/INSTANTOS.EFI" "$EFI_DIR/INSTANTOS.EFI"
cp "$INITRD" "$EFI_DIR/INITRD"

dd if=/dev/zero of="$EFI_IMG" bs=1M count=64 status=none
mformat -i "$EFI_IMG" ::
mmd -i "$EFI_IMG" ::/EFI ::/EFI/BOOT
mcopy -i "$EFI_IMG" "$BUILD_DIR/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI
mcopy -i "$EFI_IMG" "$BUILD_DIR/INSTANTOS.EFI" ::/EFI/BOOT/INSTANTOS.EFI
mcopy -i "$EFI_IMG" "$INITRD" ::/EFI/BOOT/INITRD

xorriso -as mkisofs -R -J -joliet-long -iso-level 3 \
  -eltorito-alt-boot -e EFI/BOOT/efiboot.img -no-emul-boot \
  -o "$ISO" "$ISO_ROOT" >/dev/null 2>&1

# Scratch writable root disk (FAT), matching the smoke's boot path.
DISK_IMG="$OUT_DIR/ahci.img"
rm -f "$DISK_IMG"
truncate -s 64M "$DISK_IMG"
sfdisk "$DISK_IMG" >/dev/null 2>&1 <<'EOF'
label: dos
start=2048, type=0c
EOF
mformat -F -i "$DISK_IMG@@1M" ::

echo "BASHFORK_ISO=$ISO"
echo "BASHFORK_DISK=$DISK_IMG"
echo "initrd entries: ${#ENTRIES[@]}"
