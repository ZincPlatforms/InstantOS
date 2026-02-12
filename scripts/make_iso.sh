#!/bin/bash
set -e

OUTPUT_ISO=$1
KERNEL=$2
INITRD=$3
SOURCE_DIR=$4
ISO_ROOT=$5
LIMINE_BINARY=$6

rm -rf "$ISO_ROOT"
mkdir -p "$ISO_ROOT"

cp -v "$KERNEL" "$ISO_ROOT/"
cp -v "$INITRD" "$ISO_ROOT/"

mkdir -p "$ISO_ROOT/boot"

cp -v "$SOURCE_DIR/outside/limine/limine-bios.sys" \
      "$SOURCE_DIR/outside/limine/limine-bios-cd.bin" \
      "$SOURCE_DIR/outside/limine/limine-uefi-cd.bin" \
      "$ISO_ROOT/boot/"

mkdir -p "$ISO_ROOT/EFI/BOOT"
cp -v "$SOURCE_DIR/outside/limine/BOOTX64.EFI" "$ISO_ROOT/EFI/BOOT/"
cp -v "$SOURCE_DIR/outside/limine/BOOTIA32.EFI" "$ISO_ROOT/EFI/BOOT/"

cp -v "$SOURCE_DIR/limine.conf" "$ISO_ROOT/boot/"

xorriso -as mkisofs \
    -b boot/limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot boot/limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    "$ISO_ROOT" -o "$OUTPUT_ISO"

"$LIMINE_BINARY" bios-install "$OUTPUT_ISO"

echo "ISO created: $OUTPUT_ISO"
