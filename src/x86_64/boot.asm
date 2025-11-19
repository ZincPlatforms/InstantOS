[bits 64]

global _main
extern main

_main:
    call main

.halt:
    hlt
    jmp .halt