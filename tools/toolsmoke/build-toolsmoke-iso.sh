#!/usr/bin/env bash
# Build a headless InstantOS ISO whose /bin/login is the P3.3 tool-smoke driver
# (tools/toolsmoke/toolsmoke.c): it exercises the ported GNU tar/gzip/sed/grep
# in-OS and reports TOOLSMOKE_* markers. BUILD ONLY; boot under QEMU separately
# and grep serial for the markers (TOOLSMOKE_SCORE=4/4 / TOOLSMOKE_ALL_OK).
#
# Prereqs: tools/build-mlibc.sh, and tools/build-gnu-tool.sh {gzip,sed,grep,tar}
#          (-> build/{gzip,sed,grep,tar}).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/toolsmoke}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"

MLIBC_ROOT="$BUILD_DIR/mlibc-root"
TS_DIR="$ROOT/tools/toolsmoke"

[ -f "$MLIBC_ROOT/lib/libc.so" ] || { echo "mlibc missing; run tools/build-mlibc.sh" >&2; exit 2; }
for t in gzip sed grep tar; do
  [ -f "$BUILD_DIR/$t" ] || { echo "$t missing; run tools/build-gnu-tool.sh $t" >&2; exit 2; }
done

mkdir -p "$OUT_DIR"
if [ "${SKIP_CMAKE:-0}" != "1" ]; then
  cmake --build "$BUILD_DIR" --target INSTANTOS.EFI BOOTX64.EFI mkInitrd_proj \
        input-manager storage-manager process-manager network-manager font-manager session-manager
fi

# tool-smoke driver -> /bin/login (non-mlibc freestanding, raw syscalls)
LAUNCHER="$OUT_DIR/launcher"
"$CC" --target=x86_64-unknown-elf -ffreestanding -fPIE -fno-stack-protector \
  -nostdinc -c "$TS_DIR/toolsmoke.c" -o "$OUT_DIR/toolsmoke.o"
"$LD" --gc-sections --build-id=none --hash-style=sysv -z max-page-size=0x1000 \
  -pie -e _start --dynamic-linker /lib/ld-instantos.so \
  -o "$LAUNCHER" "$OUT_DIR/toolsmoke.o"

# Host-made tar fixture for the in-OS extract (untar) test: td/f1.txt +
# td/f2.txt with contents matching toolsmoke.c's F1/F2.
FIX="$OUT_DIR/fixture"
rm -rf "$FIX"; mkdir -p "$FIX/td"
printf 'hello tar\n'    > "$FIX/td/f1.txt"
printf 'second file\n'  > "$FIX/td/f2.txt"
tar -cf "$OUT_DIR/fixture.tar" -C "$FIX" td

INITRD="$OUT_DIR/initrd.img"
ISO_ROOT="$OUT_DIR/iso"; EFI_DIR="$ISO_ROOT/EFI/BOOT"; EFI_IMG="$EFI_DIR/efiboot.img"
ISO="$OUT_DIR/toolsmoke.iso"
rm -rf "$ISO_ROOT"; mkdir -p "$EFI_DIR"

ENTRIES=(
  bin/input-manager:"$BUILD_DIR/input-manager"
  bin/storage-manager:"$BUILD_DIR/storage-manager"
  bin/process-manager:"$BUILD_DIR/process-manager"
  bin/font-manager:"$BUILD_DIR/font-manager"
  bin/session-manager:"$BUILD_DIR/session-manager"
  bin/login:"$LAUNCHER"
  bin/gzip:"$BUILD_DIR/gzip"
  bin/sed:"$BUILD_DIR/sed"
  bin/grep:"$BUILD_DIR/grep"
  bin/tar:"$BUILD_DIR/tar"
  bin/fixture.tar:"$OUT_DIR/fixture.tar"
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

# Scratch writable FAT root (the driver works in /tmp RamFS; a root disk is
# still attached to match the normal boot path).
DISK_IMG="$OUT_DIR/ahci.img"
rm -f "$DISK_IMG"; truncate -s 64M "$DISK_IMG"
printf 'label: dos\nstart=2048, type=0c\n' | sfdisk "$DISK_IMG" >/dev/null 2>&1
mformat -F -i "$DISK_IMG@@1M" ::

echo "TOOLSMOKE_ISO=$ISO"
echo "TOOLSMOKE_DISK=$DISK_IMG"
echo "initrd entries: ${#ENTRIES[@]}"
