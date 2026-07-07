#!/usr/bin/env bash
# P4 Step-B oracle ISO: boots the binutils-hosted-smoke driver (/bin/login) which
# runs the HOSTED binutils (as/ld/ar/ranlib/nm/strip, built to run in InstantOS)
# to assemble+link+archive+inspect a hello program entirely in-OS. BUILD ONLY;
# boot under QEMU and grep serial for BINUTILSHOST_* (BINUTILSHOST_ALL_OK).
#
# Prereqs: tools/build-mlibc.sh, build-binutils-cross.sh, build-binutils-hosted.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/binutils-hosted-iso}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"
MLIBC_ROOT="$BUILD_DIR/mlibc-root"
BP_DIR="$ROOT/tools/binutils-port"
HB="$BUILD_DIR/binutils-hosted-root/usr/bin"   # hosted (in-OS) binutils binaries

[ -f "$MLIBC_ROOT/lib/libc.so" ] || { echo "mlibc missing" >&2; exit 2; }
for t in as ld ar ranlib nm strip; do
  [ -f "$HB/$t" ] || { echo "hosted $t missing; run build-binutils-hosted.sh" >&2; exit 2; }
done

mkdir -p "$OUT_DIR"
if [ "${SKIP_CMAKE:-0}" != "1" ]; then
  cmake --build "$BUILD_DIR" --target INSTANTOS.EFI BOOTX64.EFI mkInitrd_proj \
        input-manager storage-manager process-manager network-manager font-manager session-manager
fi

# hello.s: clang-generated assembly (the input for the in-OS GNU as).
"$CC" --target=x86_64-unknown-elf -fPIE -fno-stack-protector -fno-addrsig \
  -nostdlibinc -isystem "$MLIBC_ROOT/include" -D_GNU_SOURCE \
  -S "$BP_DIR/hello.c" -o "$OUT_DIR/hello.s"

# smoke driver -> /bin/login (non-mlibc freestanding)
LAUNCHER="$OUT_DIR/launcher"
"$CC" --target=x86_64-unknown-elf -ffreestanding -fPIE -fno-stack-protector \
  -nostdinc -c "$BP_DIR/binutils-hosted-smoke.c" -o "$OUT_DIR/smoke.o"
"$LD" --gc-sections --build-id=none --hash-style=sysv -z max-page-size=0x1000 \
  -pie -e _start --dynamic-linker /lib/ld-instantos.so \
  -o "$LAUNCHER" "$OUT_DIR/smoke.o"

INITRD="$OUT_DIR/initrd.img"
ISO_ROOT="$OUT_DIR/iso"; EFI_DIR="$ISO_ROOT/EFI/BOOT"; EFI_IMG="$EFI_DIR/efiboot.img"
ISO="$OUT_DIR/binutils-hosted.iso"
rm -rf "$ISO_ROOT"; mkdir -p "$EFI_DIR"

ENTRIES=(
  bin/input-manager:"$BUILD_DIR/input-manager"
  bin/storage-manager:"$BUILD_DIR/storage-manager"
  bin/process-manager:"$BUILD_DIR/process-manager"
  bin/font-manager:"$BUILD_DIR/font-manager"
  bin/session-manager:"$BUILD_DIR/session-manager"
  bin/login:"$LAUNCHER"
  bin/hello.s:"$OUT_DIR/hello.s"
  bin/as:"$HB/as"
  bin/ld:"$HB/ld"
  bin/ar:"$HB/ar"
  bin/ranlib:"$HB/ranlib"
  bin/nm:"$HB/nm"
  bin/strip:"$HB/strip"
  lib/ld-instantos.so:"$BUILD_DIR/ld-instantos.so"
  lib/libinstant.so:"$BUILD_DIR/libinstant.so"
  lib/mlibc/ld-instantos.so:"$MLIBC_ROOT/lib/ld-instantos.so"
  lib/mlibc/libc.so:"$MLIBC_ROOT/lib/libc.so"
  lib/mlibc/libdl.so:"$MLIBC_ROOT/lib/libdl.so"
  lib/mlibc/crt1.o:"$MLIBC_ROOT/lib/crt1.o"
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

echo "BINUTILS_HOSTED_ISO=$ISO"
echo "BINUTILS_HOSTED_DISK=$DISK_IMG"
echo "initrd entries: ${#ENTRIES[@]}"
