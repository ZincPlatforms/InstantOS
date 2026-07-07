/* crti.o - opens the _init / _fini function frames. Standard x86-64 form; the
   body between crti and crtn is contributed by crtbegin/crtend (from libgcc).
   InstantOS/mlibc runs constructors via .init_array, so _init/_fini are mostly
   vestigial, but a well-formed frame keeps the GNU startfile spec happy. */
	.section .init,"ax",@progbits
	.global _init
	.type _init,@function
_init:
	subq $8, %rsp

	.section .fini,"ax",@progbits
	.global _fini
	.type _fini,@function
_fini:
	subq $8, %rsp

	.section .note.GNU-stack,"",@progbits
