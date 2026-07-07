#!/usr/bin/env bash
# Patch a GCC source tree to add the x86_64-unknown-instantos target (mlibc).
# Idempotent. Companion to tools/binutils-port. Adds:
#   gcc/config/instantos.h            (target macros: loader, emulation, +z now)
#   gcc/config.gcc                    (early OS defaults + x86_64 tm_file case)
#   libgcc/config.host                (crtstuff/crt parts + tls host block)
set -euo pipefail
SRC="${1:?usage: apply-gcc-instantos-port.sh <gcc-src-dir>}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC"

# Insert the contents of block-file $3 immediately before the first line of
# file $1 that contains the literal substring $2 (index(), not regex, so the
# glob-y anchors with '*' need no escaping).
insert_before() {
  awk -v s="$2" -v bf="$3" '
    (!done && index($0, s)) { while ((getline l < bf) > 0) print l; close(bf); done=1 }
    { print }
  ' "$1" > "$1.tmp" && mv "$1.tmp" "$1"
}

# 0. config.sub: recognize the instantos OS (GCC ships its own copy).
if ! grep -q 'instantos' config.sub; then
  sed -i 's/| fiwix\* )/| fiwix* | instantos* )/' config.sub
  grep -q 'instantos' config.sub && echo "patched config.sub" || { echo "config.sub anchor not found" >&2; exit 1; }
fi

# 1. target header (only copy when changed: it is a tm_file dependency, so
# touching it forces a full gcc rebuild).
if ! cmp -s "$HERE/instantos.h" gcc/config/instantos.h 2>/dev/null; then
  cp "$HERE/instantos.h" gcc/config/instantos.h
  echo "installed gcc/config/instantos.h"
fi

# 2. gcc/config.gcc
if ! grep -q 'instantos' gcc/config.gcc; then
  cat > /tmp/gcc_os_block.txt <<'BLK'
*-*-instantos*)
  extra_options="$extra_options gnu-user.opt"
  gas=yes
  gnu_ld=yes
  case ${enable_threads} in
    "" | yes | posix) thread_file=posix ;;
  esac
  tmake_file="t-slibgcc"
  default_use_cxa_atexit=yes
  use_gcc_stdint=wrap
  ;;
BLK
  cat > /tmp/gcc_cpu_block.txt <<'BLK'
x86_64-*-instantos*)
	tm_file="${tm_file} i386/unix.h i386/att.h elfos.h gnu-user.h glibc-stdint.h i386/x86-64.h i386/gnu-user-common.h i386/gnu-user64.h instantos.h"
	tmake_file="${tmake_file} i386/t-linux64"
	;;
BLK
  insert_before gcc/config.gcc '*-*-linux* | frv-*-*linux*' /tmp/gcc_os_block.txt
  insert_before gcc/config.gcc 'x86_64-*-linux* | x86_64-*-kfreebsd' /tmp/gcc_cpu_block.txt
  grep -q instantos gcc/config.gcc || { echo "config.gcc anchors not matched" >&2; exit 1; }
  echo "patched gcc/config.gcc"
fi

# 3. libgcc/config.host
if ! grep -q 'instantos' libgcc/config.host; then
  # OS-level block: PIC crtstuff (crtbegin/crtbeginS/crtend/crtendS) + shared
  # libgcc (t-slibgcc*) + EH. Mirrors the linux entry (minus t-linux).
  cat > /tmp/libgcc_os_block.txt <<'BLK'
*-*-instantos*)
	tmake_file="$tmake_file t-crtstuff-pic t-libgcc-pic t-eh-dw2-dip t-slibgcc t-slibgcc-gld t-slibgcc-elf-ver"
	extra_parts="crtbegin.o crtbeginS.o crtbeginT.o crtend.o crtendS.o"
	;;
BLK
  insert_before libgcc/config.host '*-*-linux* | frv-*-*linux* | *-*-kfreebsd*-gnu' /tmp/libgcc_os_block.txt
  # CPU-level block: x86 extra crt parts.
  cat > /tmp/libgcc_block.txt <<'BLK'
x86_64-*-instantos*)
	extra_parts="$extra_parts crtprec32.o crtprec64.o crtprec80.o crtfastmath.o"
	tmake_file="${tmake_file} i386/t-crtpc t-crtfm i386/t-crtstuff t-dfprules"
	tm_file="${tm_file} i386/elf-lib.h"
	;;
BLK
  insert_before libgcc/config.host 'x86_64-*-linux*)' /tmp/libgcc_block.txt
  # TLS / MS-ABI / shared-libgcc host block: add instantos alongside linux.
  sed -i 's/^i\[34567\]86-\*-linux\* | x86_64-\*-linux\* | \\$/i[34567]86-*-linux* | x86_64-*-linux* | x86_64-*-instantos* | \\/' libgcc/config.host
  echo "patched libgcc/config.host"
fi

# 4. libstdc++-v3 crossconfig: treat instantos like a GNU/Linux target so the
# configure probes mlibc for math/stdlib/locale support (rather than erroring
# "No support for this host/target combination"). Patch the m4 and the generated
# configure (the latter is what actually runs).
for f in libstdc++-v3/crossconfig.m4 libstdc++-v3/configure; do
  if [ -f "$f" ] && ! grep -q 'instantos' "$f"; then
    sed -i 's/\*-cygwin\* | \*-solaris\*)/*-cygwin* | *-solaris* | *-instantos*)/' "$f"
    grep -q instantos "$f" && echo "patched $f" || echo "WARN: anchor not found in $f" >&2
  fi
done

echo "OK: gcc instantos port applied to $SRC"
