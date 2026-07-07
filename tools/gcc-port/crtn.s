/* crtn.o - closes the _init / _fini function frames opened by crti.o. */
	.section .init,"ax",@progbits
	addq $8, %rsp
	ret

	.section .fini,"ax",@progbits
	addq $8, %rsp
	ret

	.section .note.GNU-stack,"",@progbits
