#!/usr/bin/env bash
# Build a headless InstantOS ISO whose /bin/login is the libc-torture harness
# (a dynamically-linked mlibc program, the Phase 2 exit gate). Also builds an
# ext4 root disk so / is a real writable filesystem. BUILD ONLY (QEMU is run
# separately on the host). Mirrors run-mlibc-smoke.sh's packaging + the ext4
# disk from build-testsuite-iso.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/libc-torture-iso}"

mkdir -p "$OUT_DIR"

if [ "${SKIP_CMAKE:-0}" != "1" ]; then
  cmake --build "$BUILD_DIR" --target INSTANTOS.EFI BOOTX64.EFI mkInitrd_proj ld-instantos \
        input-manager storage-manager process-manager network-manager font-manager session-manager
fi

# mlibc + the harness (harness links against build/mlibc-root).
"$ROOT/tools/build-mlibc.sh"
"$ROOT/tools/build-libc-torture.sh"

INITRD="$OUT_DIR/initrd.img"
ISO_ROOT="$OUT_DIR/iso"; EFI_DIR="$ISO_ROOT/EFI/BOOT"; EFI_IMG="$EFI_DIR/efiboot.img"
ISO="$OUT_DIR/libc-torture.iso"
rm -rf "$ISO_ROOT"; mkdir -p "$EFI_DIR"

TTF="$ROOT/assets/fonts/JetBrainsMono-Regular.ttf"
ENTRIES=(
  bin/login:"$BUILD_DIR/libc-torture"
  bin/input-manager:"$BUILD_DIR/input-manager"
  bin/storage-manager:"$BUILD_DIR/storage-manager"
  bin/process-manager:"$BUILD_DIR/process-manager"
  bin/network-manager:"$BUILD_DIR/network-manager"
  bin/font-manager:"$BUILD_DIR/font-manager"
  bin/session-manager:"$BUILD_DIR/session-manager"
  lib/ld-instantos.so:"$BUILD_DIR/ld-instantos.so"
  lib/libinstant.so:"$BUILD_DIR/libinstant.so"
  lib/libc.so:"$BUILD_DIR/mlibc-root/lib/libc.so"
  lib/mlibc/ld-instantos.so:"$BUILD_DIR/mlibc-root/lib/ld-instantos.so"
  lib/mlibc/libc.so:"$BUILD_DIR/mlibc-root/lib/libc.so"
)
[ -f "$TTF" ] && ENTRIES+=("bin/JetBrainsMono-Regular.ttf:$TTF")

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

# ext4 root disk (writable), seeded with /readme.txt; mounts at / like the
# os-testsuite. Gives the harness a real writable filesystem.
DISK="$OUT_DIR/ext4.img"
if [ "${SKIP_DISK:-0}" != "1" ] || [ ! -f "$DISK" ]; then
  rm -f "$DISK"; truncate -s 64M "$DISK"
  printf "label: dos\nstart=2048, type=83\n" | sfdisk "$DISK" >/dev/null 2>&1
  SEED="$(mktemp -d)"; printf 'InstantOS ext4 in-OS test seed\n' > "$SEED/readme.txt"
  mke2fs -q -F -b 4096 -O "has_journal,extent,filetype,sparse_super,^metadata_csum,^64bit,^uninit_bg,^dir_index" \
    -E offset=1048576,lazy_itable_init=0 -d "$SEED" "$DISK" 15000
fi

echo "TORTURE_ISO=$ISO"
echo "TORTURE_DISK=$DISK"
echo "initrd entries: ${#ENTRIES[@]}"
