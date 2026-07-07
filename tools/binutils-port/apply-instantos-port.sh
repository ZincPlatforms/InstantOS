#!/usr/bin/env bash
# Patch a GNU binutils source tree to add the x86_64-unknown-instantos target.
# Idempotent: safe to re-run. Mirrors the managarm (mlibc) precedent.
#
#   config.sub                     : recognize the 'instantos' OS
#   bfd/config.bfd                 : x86_64-*-instantos* -> elf64-x86-64 vector
#   gas/configure.tgt              : i386-*-instantos*  -> plain ELF
#   ld/configure.tgt               : x86_64-*-instantos* -> elf_x86_64_instantos
#   ld/emulparams/elf_x86_64_instantos.sh : /lib/ld-instantos.so interp,
#                                    4 KiB max/common page size, inherits elf_x86_64
#   ld/Makefile.in                 : register eelf_x86_64_instantos.c
#
# (hash-style sysv is set via ./configure --enable-default-hash-style=sysv)
set -euo pipefail
SRC="${1:?usage: apply-instantos-port.sh <binutils-src-dir>}"
cd "$SRC"

# 1. config.sub: add instantos to the list of recognized operating systems.
if ! grep -q 'instantos\*' config.sub; then
  sed -i 's/| fiwix\* | mlibc\* | cos\* | mbr\* )/| fiwix* | mlibc* | cos* | mbr* | instantos* )/' config.sub
  echo "patched config.sub"
fi

# 2. bfd/config.bfd: reuse the standard 64-bit x86 ELF vector.
if ! grep -q 'x86_64-\*-instantos' bfd/config.bfd; then
  sed -i 's/x86_64-\*-genode\*)/x86_64-*-genode* | x86_64-*-instantos*)/' bfd/config.bfd
  echo "patched bfd/config.bfd"
fi

# 3. gas/configure.tgt: assemble to plain ELF (x86_64 -> cpu_type i386).
if ! grep -q 'i386-\*-instantos' gas/configure.tgt; then
  awk '
    { print }
    /^  i386-\*-elf\*\)/ && !g { print "  i386-*-instantos*)\t\t\tfmt=elf ;;"; g=1 }
  ' gas/configure.tgt > gas/configure.tgt.new && mv gas/configure.tgt.new gas/configure.tgt
  echo "patched gas/configure.tgt"
fi

# 4. ld/configure.tgt: select the instantos emulation (insert before linux).
if ! grep -q 'x86_64-\*-instantos' ld/configure.tgt; then
  awk '
    /^x86_64-\*-linux-\*\)/ && !d {
      print "x86_64-*-instantos*)\ttarg_emul=elf_x86_64_instantos"
      print "\t\t\ttarg_extra_emuls=\"elf_x86_64 elf_i386\""
      print "\t\t\t;;"
      d=1
    }
    { print }
  ' ld/configure.tgt > ld/configure.tgt.new && mv ld/configure.tgt.new ld/configure.tgt
  echo "patched ld/configure.tgt"
fi

# 5. ld/emulparams: InstantOS defaults on top of elf_x86_64.
cat > ld/emulparams/elf_x86_64_instantos.sh <<'EMUL'
# InstantOS x86_64 ELF: inherit elf_x86_64, then apply InstantOS defaults so that
# `ld foo.o -lc` yields a directly-runnable binary.
#   * dynamic loader  : /lib/mlibc/ld-instantos.so  (the mlibc rtld; instantos
#                       programs are mlibc programs and link against libc.so)
#   * max/common page : 0x1000 (4 KiB)              (matches kernel + prior ld.lld flag)
source_sh ${srcdir}/emulparams/elf_x86_64.sh
MAXPAGESIZE=0x1000
COMMONPAGESIZE=0x1000
ELF_INTERPRETER_NAME=\"/lib/mlibc/ld-instantos.so\"
EMUL
echo "wrote ld/emulparams/elf_x86_64_instantos.sh"

# 6. ld/Makefile.in: register the generated emulation source.
if ! grep -q 'eelf_x86_64_instantos.c' ld/Makefile.in; then
  awk '
    { print }
    /^\teelf_x86_64.c \\$/ && !m { print "\teelf_x86_64_instantos.c \\"; m=1 }
  ' ld/Makefile.in > ld/Makefile.in.new && mv ld/Makefile.in.new ld/Makefile.in
  echo "patched ld/Makefile.in"
fi

echo "OK: instantos binutils port applied to $SRC"
