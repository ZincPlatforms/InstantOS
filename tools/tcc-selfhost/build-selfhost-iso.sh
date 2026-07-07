#!/usr/bin/env bash
# Build a headless InstantOS ISO that self-hosts TinyCC in-OS: login is replaced
# by a PTY launcher that runs /selfhost.sh, which uses /bin/tcc to recompile the
# tcc 0.9.27 sources into /tmp/tcc2 (and tcc2 -> /tmp/tcc3). This script only
# BUILDS the ISO + a scratch disk; boot it under QEMU separately and grep the
# serial log for the SELFHOST_* ladder (this host runs QEMU on Windows).
#
# Prereqs (same as tools/run-tcc-smoke.sh):
#   tools/build-mlibc.sh / build-bash.sh -> build/mlibc-root, build/bash
#   cmake --build build --target tcc     -> build/tcc, build/tcc-sysroot/...
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/tcc-selfhost}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"
CC_TARGET_FLAG="${CC_TARGET_FLAG---target=x86_64-unknown-elf}"
LAUNCHER_EXTRA_CFLAGS="${LAUNCHER_EXTRA_CFLAGS:-}"

MLIBC_ROOT="${MLIBC_ROOT:-$BUILD_DIR/mlibc-root}"
BASH_BIN="${BASH_BIN:-$BUILD_DIR/bash}"
TCC_BIN="${TCC_BIN:-$BUILD_DIR/tcc}"
TCC_SYSROOT="${TCC_SYSROOT:-$BUILD_DIR/tcc-sysroot}"
# Runtime substrate (managers + ld-instantos.so + libinstant.so). Defaults to the
# clang cmake build; the GCC world overrides SUBSTRATE_DIR + LIBINSTANT_SO.
SUBSTRATE_DIR="${SUBSTRATE_DIR:-$BUILD_DIR}"
LIBINSTANT_SO="${LIBINSTANT_SO:-$BUILD_DIR/libinstant.so}"
TCC_SRC="$ROOT/outside/iUserApps/outside/tinycc"
ILIBCXX_INC="$ROOT/outside/iUserApps/outside/ilibcxx/include"
TCC_PRIV_INC="$TCC_SRC/include"
SELFHOST_DIR="$ROOT/tools/tcc-selfhost"

[ -f "$BASH_BIN" ]                       || { echo "bash missing; run tools/build-bash.sh" >&2; exit 2; }
[ -f "$MLIBC_ROOT/lib/libc.so" ]         || { echo "mlibc missing; run tools/build-mlibc.sh" >&2; exit 2; }
[ -f "$TCC_BIN" ]                        || { echo "tcc missing; cmake --build build --target tcc" >&2; exit 2; }
[ -f "$TCC_SYSROOT/lib/tcc/libtcc1.a" ]  || { echo "libtcc1.a missing; cmake --build build --target tcc" >&2; exit 2; }
[ -f "$TCC_SRC/tcc.c" ]                  || { echo "tcc source missing; run tools/fetch-tcc.sh" >&2; exit 2; }

mkdir -p "$OUT_DIR"
if [ "${SKIP_CMAKE:-0}" != "1" ]; then
  cmake --build "$BUILD_DIR" --target INSTANTOS.EFI BOOTX64.EFI mkInitrd_proj
fi

# PTY launcher (becomes /bin/login). SELFHOST_EXT4=1 builds it to write its
# outputs to the ext4 root ("/") instead of /tmp (RamFS) -- the P1.4 "build a
# real toolchain on ext4" oracle.
LAUNCHER="$OUT_DIR/launcher"
LAUNCHER_CFLAGS=""
if [ "${SELFHOST_EXT4:-0}" = "1" ]; then LAUNCHER_CFLAGS="-DOUT_PREFIX="; fi
"$CC" $CC_TARGET_FLAG -ffreestanding -fPIE -fno-stack-protector \
  -nostdinc $LAUNCHER_CFLAGS $LAUNCHER_EXTRA_CFLAGS -c "$SELFHOST_DIR/launcher.c" -o "$OUT_DIR/launcher.o"
"$LD" --gc-sections --build-id=none --hash-style=sysv -z max-page-size=0x1000 \
  -pie -e _start --dynamic-linker /lib/ld-instantos.so \
  -o "$LAUNCHER" "$OUT_DIR/launcher.o"

INITRD="$OUT_DIR/initrd.img"
ISO_ROOT="$OUT_DIR/iso"
EFI_DIR="$ISO_ROOT/EFI/BOOT"
EFI_IMG="$EFI_DIR/efiboot.img"
ISO="$OUT_DIR/tcc-selfhost.iso"

rm -rf "$ISO_ROOT"; mkdir -p "$EFI_DIR"

# Base: managers + selfhost launcher as /bin/login + bash + mlibc runtime.
ENTRIES=(
  bin/input-manager:"$SUBSTRATE_DIR/input-manager"
  bin/storage-manager:"$SUBSTRATE_DIR/storage-manager"
  bin/process-manager:"$SUBSTRATE_DIR/process-manager"
  bin/font-manager:"$SUBSTRATE_DIR/font-manager"
  bin/session-manager:"$SUBSTRATE_DIR/session-manager"
  bin/login:"$LAUNCHER"
  bin/bash:"$BASH_BIN"
  lib/ld-instantos.so:"$SUBSTRATE_DIR/ld-instantos.so"
  lib/libinstant.so:"$LIBINSTANT_SO"
  lib/mlibc/ld-instantos.so:"$MLIBC_ROOT/lib/ld-instantos.so"
  lib/mlibc/libc.so:"$MLIBC_ROOT/lib/libc.so"
  lib/mlibc/libdl.so:"$MLIBC_ROOT/lib/libdl.so"
)

# tcc + C sysroot (CRT objects, libc.so, libtcc1.a) + the self-host script.
ENTRIES+=(
  bin/tcc:"$TCC_BIN"
  bin/tcc-hello.c:"$ROOT/tools/tcc/hello.c"
  bin/selfhost.sh:"$SELFHOST_DIR/selfhost.sh"
  lib/crt0.o:"$TCC_SYSROOT/lib/crt0.o"
  lib/crt1.o:"$TCC_SYSROOT/lib/crt1.o"
  lib/Scrt1.o:"$TCC_SYSROOT/lib/Scrt1.o"
  lib/crti.o:"$TCC_SYSROOT/lib/crti.o"
  lib/crtn.o:"$TCC_SYSROOT/lib/crtn.o"
  lib/libc.so:"$TCC_SYSROOT/lib/libc.so"
  lib/tcc/libtcc1.a:"$TCC_SYSROOT/lib/tcc/libtcc1.a"
)

# libc/POSIX headers -> /include ; tcc builtin headers -> /lib/tcc/include.
while IFS= read -r -d '' h; do
  rel="${h#"$ILIBCXX_INC"/}"
  ENTRIES+=("include/$rel:$h")
done < <(find "$ILIBCXX_INC" -type f -name '*.h' -print0)
for h in "$TCC_PRIV_INC"/*.h; do
  [ -f "$h" ] || continue
  ENTRIES+=("lib/tcc/include/$(basename "$h"):$h")
done

# The tcc sources to self-compile -> /lib/tcc/src (/lib is an initrd mount; /usr
# and / are not). The 10 compiled TUs, plus tcctools.c (#included by tcc.c) and
# every header/.def, plus our InstantOS config.h (upstream config.h is
# ./configure-generated and absent from git).
for f in tcc.c libtcc.c tccpp.c tccgen.c tccelf.c tccasm.c tccrun.c tcctools.c \
         x86_64-gen.c x86_64-link.c i386-asm.c \
         coff.h elf.h i386-asm.h i386-tok.h il-opcodes.h libtcc.h stab.def \
         stab.h tcc.h tcclib.h tcctok.h x86_64-asm.h; do
  [ -f "$TCC_SRC/$f" ] || { echo "missing tcc source: $f" >&2; exit 2; }
  ENTRIES+=("lib/tcc/src/$f:$TCC_SRC/$f")
done
ENTRIES+=("lib/tcc/src/config.h:$SELFHOST_DIR/config.instantos.h")

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

# Scratch writable root disk. Default is FAT (matching the smoke's boot path);
# SELFHOST_EXT4=1 makes it an ext4 volume so the kernel mounts ext4 at "/" and
# the compiler's outputs land on the on-disk ext4 filesystem.
DISK_IMG="$OUT_DIR/ahci.img"
rm -f "$DISK_IMG"
truncate -s 64M "$DISK_IMG"
if [ "${SELFHOST_EXT4:-0}" = "1" ]; then
  printf 'label: dos\nstart=2048, type=83\n' | sfdisk "$DISK_IMG" >/dev/null 2>&1
  mke2fs -q -F -b 4096 -O "has_journal,extent,filetype,sparse_super,^metadata_csum,^64bit,^uninit_bg,^dir_index" \
    -E offset=1048576,lazy_itable_init=0 "$DISK_IMG" 15000
else
  sfdisk "$DISK_IMG" >/dev/null 2>&1 <<'EOF'
label: dos
start=2048, type=0c
EOF
  mformat -F -i "$DISK_IMG@@1M" ::
fi

echo "SELFHOST_ISO=$ISO"
echo "SELFHOST_DISK=$DISK_IMG"
echo "initrd entries: ${#ENTRIES[@]}"
