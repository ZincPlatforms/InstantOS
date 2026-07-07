#!/usr/bin/env bash
# Build a headless InstantOS ISO whose /bin/login runs /bin/hello -- a program
# assembled by GNU `as` and linked by GNU `ld` (x86_64-unknown-instantos cross
# tools) against mlibc. Proves the P4 Step-A oracle: GNU-toolchain output loads
# (ld-instantos accepts it) and runs in-OS. BUILD ONLY; boot under QEMU and grep
# serial for BINUTILS_* markers (BINUTILS_ALL_OK).
#
# Prereqs: tools/build-mlibc.sh, tools/binutils-port/build-binutils-cross.sh
#          (-> build/binutils-cross/bin/x86_64-unknown-instantos-{as,ld}).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/binutils-hello}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"

MLIBC_ROOT="$BUILD_DIR/mlibc-root"
BP_DIR="$ROOT/tools/binutils-port"
BX="$BUILD_DIR/binutils-cross/bin"
GAS="$BX/x86_64-unknown-instantos-as"
GLD="$BX/x86_64-unknown-instantos-ld"

[ -f "$MLIBC_ROOT/lib/libc.so" ] || { echo "mlibc missing; run tools/build-mlibc.sh" >&2; exit 2; }
[ -x "$GAS" ] || { echo "GNU as missing; run tools/binutils-port/build-binutils-cross.sh" >&2; exit 2; }
[ -x "$GLD" ] || { echo "GNU ld missing; run tools/binutils-port/build-binutils-cross.sh" >&2; exit 2; }

mkdir -p "$OUT_DIR"
if [ "${SKIP_CMAKE:-0}" != "1" ]; then
  cmake --build "$BUILD_DIR" --target INSTANTOS.EFI BOOTX64.EFI mkInitrd_proj \
        input-manager storage-manager process-manager network-manager font-manager session-manager
fi

# hello: clang -S (mlibc headers) -> GNU as -> GNU ld (PIE, mlibc crt1 + -lc).
# -fno-addrsig keeps clang from emitting the LLVM .addrsig directives GNU as
# does not understand.
"$CC" --target=x86_64-unknown-elf -fPIE -fno-stack-protector -fno-addrsig \
  -nostdlibinc -isystem "$MLIBC_ROOT/include" -D_GNU_SOURCE \
  -S "$BP_DIR/hello.c" -o "$OUT_DIR/hello.s"
"$GAS" "$OUT_DIR/hello.s" -o "$OUT_DIR/hello.o"
# mlibc programs use the mlibc dynamic loader at /lib/mlibc/ld-instantos.so with
# rpath /lib/mlibc (where libc.so lives) -- exactly as the lld-built userland
# does. -z now (eager binding) matches the BIND_NOW convention of that userland.
# (The elf_x86_64_instantos emulation bakes these as defaults; passed explicitly
# here so this test does not depend on a binutils rebuild.)
"$GLD" -pie -z now --dynamic-linker /lib/mlibc/ld-instantos.so -rpath /lib/mlibc \
  -o "$OUT_DIR/hello" "$MLIBC_ROOT/lib/crt1.o" "$OUT_DIR/hello.o" \
  -L "$MLIBC_ROOT/lib" -lc

# smoke driver -> /bin/login (non-mlibc freestanding, raw syscalls)
LAUNCHER="$OUT_DIR/launcher"
"$CC" --target=x86_64-unknown-elf -ffreestanding -fPIE -fno-stack-protector \
  -nostdinc -c "$BP_DIR/binutils-smoke.c" -o "$OUT_DIR/smoke.o"
"$LD" --gc-sections --build-id=none --hash-style=sysv -z max-page-size=0x1000 \
  -pie -e _start --dynamic-linker /lib/ld-instantos.so \
  -o "$LAUNCHER" "$OUT_DIR/smoke.o"

INITRD="$OUT_DIR/initrd.img"
ISO_ROOT="$OUT_DIR/iso"; EFI_DIR="$ISO_ROOT/EFI/BOOT"; EFI_IMG="$EFI_DIR/efiboot.img"
ISO="$OUT_DIR/binutils-hello.iso"
rm -rf "$ISO_ROOT"; mkdir -p "$EFI_DIR"

ENTRIES=(
  bin/input-manager:"$BUILD_DIR/input-manager"
  bin/storage-manager:"$BUILD_DIR/storage-manager"
  bin/process-manager:"$BUILD_DIR/process-manager"
  bin/font-manager:"$BUILD_DIR/font-manager"
  bin/session-manager:"$BUILD_DIR/session-manager"
  bin/login:"$LAUNCHER"
  bin/hello:"$OUT_DIR/hello"
  lib/ld-instantos.so:"$BUILD_DIR/ld-instantos.so"
  lib/libinstant.so:"$BUILD_DIR/libinstant.so"
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

DISK_IMG="$OUT_DIR/ahci.img"
rm -f "$DISK_IMG"; truncate -s 64M "$DISK_IMG"
printf 'label: dos\nstart=2048, type=0c\n' | sfdisk "$DISK_IMG" >/dev/null 2>&1
mformat -F -i "$DISK_IMG@@1M" ::

echo "BINUTILS_HELLO_ISO=$ISO"
echo "BINUTILS_HELLO_DISK=$DISK_IMG"
echo "initrd entries: ${#ENTRIES[@]}"
