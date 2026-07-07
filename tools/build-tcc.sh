#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TCC_SOURCE_DIR="${TCC_SOURCE_DIR:-$ROOT/outside/iUserApps/outside/tinycc}"
TCC_BUILD_DIR="${TCC_BUILD_DIR:-$ROOT/build/tcc-build}"
TCC_SYSROOT="${TCC_SYSROOT:-$ROOT/build/tcc-sysroot}"
TCC_OUTPUT="${TCC_OUTPUT:-$ROOT/build/tcc}"
CC="${CC:-clang}"
LD="${LD:-ld.lld}"
AR="${AR:-llvm-ar}"
STRIP="${STRIP:-llvm-strip}"

# Compile/link flag sets baked into the generated cc wrapper. Overridable so the
# same script drives the clang cross (default) or the GCC cross
# (tools/build-tcc-gcc.sh): the latter drops --target/-fuse-ld=lld and adds
# GCC's freestanding include dir.
TCC_WRAP_CFLAGS="${TCC_WRAP_CFLAGS:---target=x86_64-unknown-elf -fPIC -ffreestanding -fno-stack-protector -nostdinc -isystem $TCC_SYSROOT/include}"
TCC_WRAP_LDFLAGS="${TCC_WRAP_LDFLAGS:--nostdlib -fuse-ld=lld -Wl,--gc-sections -Wl,--build-id=none -Wl,--hash-style=sysv -Wl,-z,max-page-size=0x1000 -pie -Wl,-e,_start -Wl,--dynamic-linker,/lib/ld-instantos.so}"
# Space-separated -l tokens to strip from the final link (default none). The GCC
# build sets "-lm -ldl": libinstant already provides those symbols, so linking
# them would drag mlibc's libc.so/libdl.so into a libinstant binary.
TCC_WRAP_DROP_LIBS="${TCC_WRAP_DROP_LIBS:-}"

# Rebuild the libinstant tcc-sysroot unless told to reuse an existing one
# (TCC_SKIP_SYSROOT=1) -- e.g. the GCC tcc build reuses the clang-built sysroot
# and only recompiles tcc itself, so the sysroot builder must not inherit the
# GCC CC/flags.
if [ "${TCC_SKIP_SYSROOT:-0}" != "1" ]; then
  BUILD_DIR="${BUILD_DIR:-$ROOT/build}" TCC_SYSROOT="$TCC_SYSROOT" "$ROOT/tools/build-tcc-sysroot.sh"
fi

if [ ! -f "$TCC_SOURCE_DIR/configure" ]; then
  printf 'tcc source not found: %s\n' "$TCC_SOURCE_DIR" >&2
  printf 'run tools/fetch-tcc.sh first or set TCC_SOURCE_DIR\n' >&2
  exit 2
fi

if [ "${TCC_NATIVE_BUILD:-0}" != "1" ]; then
  printf 'TCC sysroot prepared. Native TCC cross-build is intentionally gated.\n'
  printf 'Set TCC_NATIVE_BUILD=1 to run the experimental upstream build step.\n'
  printf 'Expected next work: add InstantOS target patches under tools/tcc/patches.\n'
  exit 0
fi

case "$TCC_BUILD_DIR" in
  ""|"/"|"$ROOT")
    printf 'refusing unsafe TCC_BUILD_DIR: %s\n' "$TCC_BUILD_DIR" >&2
    exit 2
    ;;
esac
rm -rf "$TCC_BUILD_DIR"
mkdir -p "$TCC_BUILD_DIR"

TCC_CC_WRAPPER="$TCC_BUILD_DIR/instantos-cc"
TCC_AR_WRAPPER="$TCC_BUILD_DIR/instantos-ar"
TCC_STRIP_WRAPPER="$TCC_BUILD_DIR/instantos-strip"
cat > "$TCC_CC_WRAPPER" <<EOF
#!/usr/bin/env bash
set -euo pipefail

compile_only=0
for arg in "\$@"; do
  case "\$arg" in
    -c|-E|-S)
      compile_only=1
      ;;
  esac
done

common=(
  "$CC"
  $TCC_WRAP_CFLAGS
)

if [ "\$compile_only" = "1" ]; then
  exec "\${common[@]}" "\$@"
fi

# Strip mlibc-world libs (baked from TCC_WRAP_DROP_LIBS); libinstant provides
# their symbols via -linstant below.
drop="$TCC_WRAP_DROP_LIBS"
link_args=()
for a in "\$@"; do
  skip=0
  for d in \$drop; do
    if [ "\$a" = "\$d" ]; then skip=1; break; fi
  done
  [ "\$skip" = "1" ] || link_args+=("\$a")
done

exec "\${common[@]}" \
  $TCC_WRAP_LDFLAGS \
  "$TCC_SYSROOT/lib/crt0.o" \
  "\${link_args[@]}" \
  -L"$TCC_SYSROOT/lib" \
  -linstant
EOF
chmod +x "$TCC_CC_WRAPPER"

cat > "$TCC_AR_WRAPPER" <<EOF
#!/usr/bin/env bash
exec "$AR" "\$@"
EOF
chmod +x "$TCC_AR_WRAPPER"

cat > "$TCC_STRIP_WRAPPER" <<EOF
#!/usr/bin/env bash
if command -v "$STRIP" >/dev/null 2>&1; then
  exec "$STRIP" "\$@"
fi
exit 0
EOF
chmod +x "$TCC_STRIP_WRAPPER"

printf 'experimental: configuring TCC for InstantOS using sysroot %s\n' "$TCC_SYSROOT"
(
  cd "$TCC_BUILD_DIR"
  "$TCC_SOURCE_DIR/configure" \
    --prefix=/usr \
    --tccdir=/lib/tcc \
    --cpu=x86_64 \
    --cross-prefix="$TCC_BUILD_DIR/instantos-" \
    --cc=cc \
    --ar=ar \
    --enable-static \
    --extra-cflags="-Wall -g -O2 -DCONFIG_TCC_STATIC -DCONFIG_TCCBOOT" \
    --crtprefix=/lib \
    --sysincludepaths=/lib/tcc/include:/include:/usr/include \
    --libpaths=/lib/tcc:/lib:/usr/lib \
    --elfinterp=/lib/ld-instantos.so
  make ${TCC_MAKE_TARGETS:-tcc}
  make -C lib clean
  make -C lib x86_64-libtcc1-usegcc=yes BCHECK_O=
)

if [ -f "$TCC_BUILD_DIR/tcc" ]; then
  cp "$TCC_BUILD_DIR/tcc" "$TCC_OUTPUT"
  printf 'tcc built: %s\n' "$TCC_OUTPUT"
else
  printf 'tcc build completed without expected output: %s/tcc\n' "$TCC_BUILD_DIR" >&2
  exit 1
fi

if [ -f "$TCC_BUILD_DIR/libtcc1.a" ]; then
  mkdir -p "$TCC_SYSROOT/lib/tcc"
  cp "$TCC_BUILD_DIR/libtcc1.a" "$TCC_SYSROOT/lib/tcc/libtcc1.a"
  cp "$TCC_BUILD_DIR/libtcc1.a" "$TCC_SYSROOT/lib/libtcc1.a"
  printf 'libtcc1 built: %s\n' "$TCC_SYSROOT/lib/tcc/libtcc1.a"
else
  printf 'libtcc1 build completed without expected output: %s/libtcc1.a\n' "$TCC_BUILD_DIR" >&2
  exit 1
fi
