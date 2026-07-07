#!/usr/bin/env bash
# P4 Step-A oracle #2: relink the existing 86-check libc-torture harness with the
# GNU x86_64-unknown-instantos toolchain (clang -S -> GNU as -> GNU ld) instead
# of ld.lld, and run it in-OS. Proves ld-instantos loads a large, real GNU-ld
# binary (dozens of PLT/dynamic relocations) and it is functionally correct.
# BUILD ONLY; boot under QEMU and grep serial for "[libctorture] SCORE" / "END".
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/libc-torture-gnu-iso}"
CC="${CC:-clang}"
MLIBC_ROOT="$BUILD_DIR/mlibc-root"
SRC="$ROOT/outside/iUserApps/libc-torture/src"
BX="$BUILD_DIR/binutils-cross/bin"
GAS="$BX/x86_64-unknown-instantos-as"
GLD="$BX/x86_64-unknown-instantos-ld"

[ -f "$MLIBC_ROOT/lib/libc.so" ] || { echo "mlibc missing; run tools/build-mlibc.sh" >&2; exit 2; }
[ -x "$GAS" ] && [ -x "$GLD" ] || { echo "GNU as/ld missing; build binutils-cross first" >&2; exit 2; }

mkdir -p "$OUT_DIR"
if [ "${SKIP_CMAKE:-0}" != "1" ]; then
  cmake --build "$BUILD_DIR" --target INSTANTOS.EFI BOOTX64.EFI mkInitrd_proj ld-instantos \
        input-manager storage-manager process-manager network-manager font-manager session-manager
fi

# Relink libc-torture with the GNU toolchain: clang -S (mlibc headers) -> GNU as
# for main.c; GNU as for the plain-asm start.S; GNU ld (PIE, -z now eager binding,
# mlibc loader via baked default + rpath /lib/mlibc, -lc).
TORTURE="$OUT_DIR/libc-torture-gnu"
"$CC" --target=x86_64-unknown-elf -fPIE -fno-stack-protector -fno-addrsig \
  -nostdinc -isystem "$MLIBC_ROOT/include" -S "$SRC/main.c" -o "$OUT_DIR/torture.s"
"$GAS" "$OUT_DIR/torture.s" -o "$OUT_DIR/torture.o"
"$GAS" "$SRC/start.S" -o "$OUT_DIR/torture-start.o"
"$GLD" -pie -z now -e _start -rpath /lib/mlibc -o "$TORTURE" \
  "$OUT_DIR/torture-start.o" "$OUT_DIR/torture.o" -L "$MLIBC_ROOT/lib" -lc

INITRD="$OUT_DIR/initrd.img"
ISO_ROOT="$OUT_DIR/iso"; EFI_DIR="$ISO_ROOT/EFI/BOOT"; EFI_IMG="$EFI_DIR/efiboot.img"
ISO="$OUT_DIR/libc-torture-gnu.iso"
rm -rf "$ISO_ROOT"; mkdir -p "$EFI_DIR"

ENTRIES=(
  bin/login:"$TORTURE"
  bin/input-manager:"$BUILD_DIR/input-manager"
  bin/storage-manager:"$BUILD_DIR/storage-manager"
  bin/process-manager:"$BUILD_DIR/process-manager"
  bin/network-manager:"$BUILD_DIR/network-manager"
  bin/font-manager:"$BUILD_DIR/font-manager"
  bin/session-manager:"$BUILD_DIR/session-manager"
  lib/ld-instantos.so:"$BUILD_DIR/ld-instantos.so"
  lib/libinstant.so:"$BUILD_DIR/libinstant.so"
  lib/libc.so:"$MLIBC_ROOT/lib/libc.so"
  lib/mlibc/ld-instantos.so:"$MLIBC_ROOT/lib/ld-instantos.so"
  lib/mlibc/libc.so:"$MLIBC_ROOT/lib/libc.so"
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

# ext4 root disk (writable), seeded with /readme.txt -- matches the baseline
# libc-torture ISO so the file-mtime coherence check behaves as on ext4.
DISK_IMG="$OUT_DIR/ext4.img"
rm -f "$DISK_IMG"; truncate -s 64M "$DISK_IMG"
printf 'label: dos\nstart=2048, type=83\n' | sfdisk "$DISK_IMG" >/dev/null 2>&1
SEED="$(mktemp -d)"; printf 'InstantOS ext4 in-OS test seed\n' > "$SEED/readme.txt"
mke2fs -q -F -b 4096 -O "has_journal,extent,filetype,sparse_super,^metadata_csum,^64bit,^uninit_bg,^dir_index" \
  -E offset=1048576,lazy_itable_init=0 -d "$SEED" "$DISK_IMG" 15000

echo "TORTURE_GNU_ISO=$ISO"
echo "TORTURE_GNU_DISK=$DISK_IMG"
echo "initrd entries: ${#ENTRIES[@]}"
