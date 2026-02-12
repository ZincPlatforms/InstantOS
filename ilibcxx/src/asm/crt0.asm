bits 64

extern main
extern _init
extern _fini
extern i_onexit
extern __cxa_finalize

global _start

global memset32
memset32:
    mov eax, esi
    mov rcx, rdx

    rep stosd
    
    mov rax, rdi
    ret

section .text
_start:
    xor rbp, rbp
    
    pop rdi
    mov rsi, rsp
    
    and rsp, ~0xF
    
    call _init
    
    call main
    
    mov rdi, rax
    
    call _fini

    mov rax, rdi

    call i_onexit
    
    mov rax, 3
    int 0x80

.loop:
    jmp .loop
    
    hlt
