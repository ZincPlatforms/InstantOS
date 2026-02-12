bits 64

global syscall0
global syscall1
global syscall2
global syscall3
global syscall4
global syscall5

syscall0:
    mov rax, rdi
    int 0x80
    ret

syscall1:
    mov rax, rdi
    mov rbx, rsi
    int 0x80
    ret

syscall2:
    mov rax, rdi
    mov rbx, rsi
    mov rcx, rdx
    int 0x80
    ret

syscall3:
    mov rax, rdi
    mov rbx, rsi
    mov r10, rcx
    mov rcx, rdx
    mov rdx, r10
    int 0x80
    ret

syscall4:
    mov rax, rdi
    mov rbx, rsi
    mov r10, rcx
    mov rcx, rdx
    mov rdx, r10
    mov rsi, r8
    int 0x80
    ret

syscall5:
    mov rax, rdi
    mov rbx, rsi
    mov r10, rcx
    mov rcx, rdx
    mov rdx, r10
    mov rsi, r8
    mov rdi, r9
    int 0x80
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
