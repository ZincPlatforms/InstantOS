#!/bin/bash
# In-OS TinyCC self-host ladder. Driven by the launcher over a PTY; only bash
# builtins + /bin/tcc are available (no coreutils). Each rung prints a sentinel
# so the runner can pinpoint the first failure.
#
#   stage2: /bin/tcc  (built by clang, cross-targeted to InstantOS) compiles the
#           tcc 0.9.27 sources -> /tmp/tcc2
#   verify: tcc2 is an ELF, runs (-v), and can itself compile+run a program
#   stage3: tcc2 compiles the same sources again -> /tmp/tcc3  (proves tcc2 is a
#           fully working compiler, i.e. real self-hosting)

S=/lib/tcc/src
DEFS="-DTCC_TARGET_X86_64 -DONE_SOURCE=0 -DCONFIG_TCC_STATIC -DCONFIG_TCCBOOT"
SRCS="$S/tcc.c $S/libtcc.c $S/tccpp.c $S/tccgen.c $S/tccelf.c $S/tccasm.c $S/tccrun.c $S/x86_64-gen.c $S/x86_64-link.c $S/i386-asm.c"

echo "SELFHOST_START"
cd /tmp || { echo "SELFHOST_NO_TMP"; exit 1; }

is_elf() { local m; read -n4 m < "$1" 2>/dev/null; [[ "${m:1:3}" == "ELF" ]]; }

echo "SELFHOST_STAGE2_BEGIN"
tcc $DEFS -I"$S" -o /tmp/tcc2 $SRCS
echo "SELFHOST_STAGE2_RC=$?"
[[ -s /tmp/tcc2 ]] && echo "SELFHOST_TCC2_EXISTS"
if is_elf /tmp/tcc2; then echo "SELFHOST_TCC2_ELF_OK"; else echo "SELFHOST_TCC2_ELF_BAD"; fi

echo "SELFHOST_TCC2_VERSION_BEGIN"
/tmp/tcc2 -v
echo "SELFHOST_TCC2_VERSION_RC=$?"

echo "SELFHOST_HELLO_BEGIN"
/tmp/tcc2 /bin/tcc-hello.c -o /tmp/hello2
echo "SELFHOST_HELLO_CC_RC=$?"
if is_elf /tmp/hello2; then echo "SELFHOST_HELLO_ELF_OK"; else echo "SELFHOST_HELLO_ELF_BAD"; fi
/tmp/hello2
echo "SELFHOST_HELLO_RUN_RC=$?"

echo "SELFHOST_STAGE3_BEGIN"
/tmp/tcc2 $DEFS -I"$S" -o /tmp/tcc3 $SRCS
echo "SELFHOST_STAGE3_RC=$?"
[[ -s /tmp/tcc3 ]] && echo "SELFHOST_TCC3_EXISTS"
if is_elf /tmp/tcc3; then echo "SELFHOST_TCC3_ELF_OK"; else echo "SELFHOST_TCC3_ELF_BAD"; fi

echo "SELFHOST_DONE"
exit 0
