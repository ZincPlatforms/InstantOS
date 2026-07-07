#!/usr/bin/env bash
# Build a headless InstantOS ISO whose /bin/login is the buildworld smoke driver
# (tools/buildworld/launcher.c): it runs GNU make building a small multi-file C
# project with tcc, then an incremental rebuild. BUILD ONLY; boot under QEMU
# separately and grep serial for BUILDWORLD_* markers.
#
# Prereqs: tools/build-mlibc.sh, tools/build-bash.sh, tools/build-make.sh,
#          cmake --build build --target tcc  (-> build/tcc, build/tcc-sysroot).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
OUT_DIR="${OUT_DIR:-$BUILD_DIR/buildworld}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"

MLIBC_ROOT="$BUILD_DIR/mlibc-root"
BASH_BIN="$BUILD_DIR/bash"
MAKE_BIN="$BUILD_DIR/make"
TCC_BIN="$BUILD_DIR/tcc"
TCC_SYSROOT="$BUILD_DIR/tcc-sysroot"
ILIBCXX_INC="$ROOT/outside/iUserApps/outside/ilibcxx/include"
TCC_SRC="$ROOT/outside/iUserApps/outside/tinycc"
TCC_PRIV_INC="$TCC_SRC/include"
BW_DIR="$ROOT/tools/buildworld"

[ -f "$MLIBC_ROOT/lib/libc.so" ]        || { echo "mlibc missing; run tools/build-mlibc.sh" >&2; exit 2; }
[ -f "$BASH_BIN" ]                      || { echo "bash missing; run tools/build-bash.sh" >&2; exit 2; }
[ -f "$MAKE_BIN" ]                      || { echo "make missing; run tools/build-make.sh" >&2; exit 2; }
[ -f "$TCC_BIN" ]                       || { echo "tcc missing; cmake --build build --target tcc" >&2; exit 2; }
[ -f "$TCC_SYSROOT/lib/tcc/libtcc1.a" ] || { echo "libtcc1.a missing; cmake --build build --target tcc" >&2; exit 2; }
for t in gzip sed grep tar; do
  [ -f "$BUILD_DIR/$t" ] || { echo "$t missing; run tools/build-gnu-tool.sh $t" >&2; exit 2; }
done

mkdir -p "$OUT_DIR"
if [ "${SKIP_CMAKE:-0}" != "1" ]; then
  cmake --build "$BUILD_DIR" --target INSTANTOS.EFI BOOTX64.EFI mkInitrd_proj \
        input-manager storage-manager process-manager network-manager font-manager session-manager
fi

# buildworld driver -> /bin/login
LAUNCHER="$OUT_DIR/launcher"
"$CC" --target=x86_64-unknown-elf -ffreestanding -fPIE -fno-stack-protector \
  -nostdinc -c "$BW_DIR/launcher.c" -o "$OUT_DIR/launcher.o"
"$LD" --gc-sections --build-id=none --hash-style=sysv -z max-page-size=0x1000 \
  -pie -e _start --dynamic-linker /lib/ld-instantos.so \
  -o "$LAUNCHER" "$OUT_DIR/launcher.o"

# fd-inheritance test -> /bin/fdtest (mlibc-linked; self-execs to prove fd>2
# survives exec).
FDTEST="$OUT_DIR/fdtest"
"$CC" --target=x86_64-unknown-elf -ffreestanding -fPIE -fno-stack-protector \
  -nostdinc -isystem "$MLIBC_ROOT/include" -c "$BW_DIR/fdtest.c" -o "$OUT_DIR/fdtest.o"
"$CC" --target=x86_64-unknown-elf -ffreestanding -fPIE -c "$BW_DIR/start.S" -o "$OUT_DIR/fdtest-start.o"
"$LD" --gc-sections --build-id=none --hash-style=sysv -z max-page-size=0x1000 \
  -pie -e _start --dynamic-linker /lib/mlibc/ld-instantos.so -rpath /lib/mlibc \
  -o "$FDTEST" "$OUT_DIR/fdtest-start.o" "$OUT_DIR/fdtest.o" -L "$MLIBC_ROOT/lib" -lc

# Host-made project tarball (proj/) that the launcher unpacks + configures in-OS.
# configure-lite is a real /bin/sh script: it runs a tcc compile-test, a grep
# version check, then sed-substitutes @CC@ in Makefile.in -> Makefile. Recipe
# lines in Makefile.in use TABS (printf \t).
PROJ="$OUT_DIR/proj"
rm -rf "$PROJ"; mkdir -p "$PROJ"
printf 'extern int a(void); extern int b(void);\nint main(void){ return a()+b()-7; }\n' > "$PROJ/main.c"
printf 'int a(void){ return 3; }\n'  > "$PROJ/a.c"
printf 'int b(void){ return 4; }\n'  > "$PROJ/b.c"
printf 'CC = @CC@\napp: main.o a.o b.o\n\t$(CC) -o $@ $^\n%%.o: %%.c\n\t$(CC) -c $< -o $@\n' > "$PROJ/Makefile.in"
cat > "$PROJ/configure" <<'CFG'
#!/bin/sh
# configure-lite: detect and sanity-check the C compiler, then generate Makefile.
CC=tcc
echo "configure: checking for C compiler... $CC"
printf 'int main(void){return 0;}\n' > conftest.c
if $CC -c conftest.c -o conftest.o; then
  echo "configure: the C compiler works"
else
  echo "configure: error: C compiler cannot create object files" >&2
  exit 1
fi
if $CC -v 2>&1 | grep -q version; then
  echo "configure: compiler version check ok"
fi
echo "configure: creating Makefile"
sed "s|@CC@|$CC|g" Makefile.in > Makefile
echo "configure: done"
CFG
chmod +x "$PROJ/configure"
tar -czf "$OUT_DIR/proj.tar.gz" -C "$OUT_DIR" proj

INITRD="$OUT_DIR/initrd.img"
ISO_ROOT="$OUT_DIR/iso"; EFI_DIR="$ISO_ROOT/EFI/BOOT"; EFI_IMG="$EFI_DIR/efiboot.img"
ISO="$OUT_DIR/buildworld.iso"
rm -rf "$ISO_ROOT"; mkdir -p "$EFI_DIR"

ENTRIES=(
  bin/input-manager:"$BUILD_DIR/input-manager"
  bin/storage-manager:"$BUILD_DIR/storage-manager"
  bin/process-manager:"$BUILD_DIR/process-manager"
  bin/font-manager:"$BUILD_DIR/font-manager"
  bin/session-manager:"$BUILD_DIR/session-manager"
  bin/login:"$LAUNCHER"
  bin/make:"$MAKE_BIN"
  bin/bash:"$BASH_BIN"
  bin/sh:"$BASH_BIN"
  bin/tcc:"$TCC_BIN"
  bin/gzip:"$BUILD_DIR/gzip"
  bin/sed:"$BUILD_DIR/sed"
  bin/grep:"$BUILD_DIR/grep"
  bin/tar:"$BUILD_DIR/tar"
  bin/proj.tar.gz:"$OUT_DIR/proj.tar.gz"
  bin/fdtest:"$FDTEST"
  lib/ld-instantos.so:"$BUILD_DIR/ld-instantos.so"
  lib/libinstant.so:"$BUILD_DIR/libinstant.so"
  lib/mlibc/ld-instantos.so:"$MLIBC_ROOT/lib/ld-instantos.so"
  lib/mlibc/libc.so:"$MLIBC_ROOT/lib/libc.so"
  lib/mlibc/libdl.so:"$MLIBC_ROOT/lib/libdl.so"
  # tcc C sysroot: CRT objects, libc.so, libtcc1.a
  lib/crt0.o:"$TCC_SYSROOT/lib/crt0.o"
  lib/crt1.o:"$TCC_SYSROOT/lib/crt1.o"
  lib/Scrt1.o:"$TCC_SYSROOT/lib/Scrt1.o"
  lib/crti.o:"$TCC_SYSROOT/lib/crti.o"
  lib/crtn.o:"$TCC_SYSROOT/lib/crtn.o"
  lib/libc.so:"$TCC_SYSROOT/lib/libc.so"
  lib/tcc/libtcc1.a:"$TCC_SYSROOT/lib/tcc/libtcc1.a"
)
# libc/POSIX headers -> /include ; tcc builtin headers -> /lib/tcc/include
while IFS= read -r -d '' h; do
  rel="${h#"$ILIBCXX_INC"/}"
  ENTRIES+=("include/$rel:$h")
done < <(find "$ILIBCXX_INC" -type f -name '*.h' -print0)
for h in "$TCC_PRIV_INC"/*.h; do
  [ -f "$h" ] || continue
  ENTRIES+=("lib/tcc/include/$(basename "$h"):$h")
done

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

# Scratch writable FAT root (the project builds in /tmp RamFS; a root disk is
# still attached to match the normal boot path).
DISK_IMG="$OUT_DIR/ahci.img"
rm -f "$DISK_IMG"; truncate -s 64M "$DISK_IMG"
printf 'label: dos\nstart=2048, type=0c\n' | sfdisk "$DISK_IMG" >/dev/null 2>&1
mformat -F -i "$DISK_IMG@@1M" ::

echo "BUILDWORLD_ISO=$ISO"
echo "BUILDWORLD_DISK=$DISK_IMG"
echo "initrd entries: ${#ENTRIES[@]}"
