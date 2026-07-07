/* Definitions for x86_64-unknown-instantos (InstantOS, mlibc userland).
   Included last in the tm_file chain after the generic gnu-user + i386 headers;
   overrides the GNU/Linux defaults with the InstantOS runtime conventions. */

#undef  TARGET_INSTANTOS
#define TARGET_INSTANTOS 1

/* InstantOS programs are mlibc programs: use the mlibc dynamic loader. All ABI
   variants collapse to the single 64-bit loader (we build 64-bit only). */
#undef  GNU_USER_DYNAMIC_LINKER
#define GNU_USER_DYNAMIC_LINKER    "/lib/mlibc/ld-instantos.so"
#undef  GNU_USER_DYNAMIC_LINKER32
#define GNU_USER_DYNAMIC_LINKER32  "/lib/mlibc/ld-instantos.so"
#undef  GNU_USER_DYNAMIC_LINKER64
#define GNU_USER_DYNAMIC_LINKER64  "/lib/mlibc/ld-instantos.so"
#undef  GNU_USER_DYNAMIC_LINKERX32
#define GNU_USER_DYNAMIC_LINKERX32 "/lib/mlibc/ld-instantos.so"

/* Link with the InstantOS ld emulation: it bakes the loader path and a 4 KiB
   max page size (matching the kernel + ld-instantos). Defined in the binutils
   port (ld/emulparams/elf_x86_64_instantos.sh). */
#undef  GNU_USER_LINK_EMULATION64
#define GNU_USER_LINK_EMULATION64  "elf_x86_64_instantos"
#undef  GNU_USER_LINK_EMULATION32
#define GNU_USER_LINK_EMULATION32  "elf_i386"
#undef  GNU_USER_LINK_EMULATIONX32
#define GNU_USER_LINK_EMULATIONX32 "elf32_x86_64"

/* Eager binding: ld-instantos resolves relocations at load time; the whole
   InstantOS userland is BIND_NOW. */
#undef  LINK_SPEC
#define LINK_SPEC GNU_USER_TARGET_LINK_SPEC " -z now"

/* Target OS preprocessor macros. */
#undef  TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()			\
  do {							\
      builtin_define ("__instantos__");			\
      builtin_define ("__InstantOS__");			\
      builtin_define ("__unix__");			\
      builtin_assert ("system=instantos");		\
      builtin_assert ("system=unix");			\
      builtin_assert ("system=posix");			\
  } while (0)

/* mlibc does not place the stack-protector guard at a fixed %fs offset the way
   glibc does; fall back to the generic global __stack_chk_guard. */
#undef TARGET_LIBC_PROVIDES_SSP

/* mlibc provides __cxa_atexit. */
#undef  TARGET_DEFAULT_LONG_DOUBLE_128
