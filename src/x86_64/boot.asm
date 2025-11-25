[bits 64]

global _main
extern initConstructors
extern _kinit

global __dso_handle
__dso_handle:
    dq 0

global __cxa_atexit
__cxa_atexit:
    xor eax, eax
    ret

__cxa_pure_virtual:
    hlt
    jmp __cxa_pure_virtual

global __cxa_guard_acquire
__cxa_guard_acquire:
    push rbp
    mov rbp, rsp
    
    mov rax, rdi

    cmp byte [rax], 0
    jne .initted
    
    mov dl, 1
    xchg [rax], dl
    cmp dl, 0
    je .acquired
    
.initted:
    xor eax, eax
    jmp .done
    
.acquired:
    mov eax, 1
    
.done:
    pop rbp
    ret

global __cxa_guard_release
__cxa_guard_release:
    push rbp
    mov rbp, rsp
    
    mov rax, rdi

    mov byte [rax], 1
    
    pop rbp
    ret

global __cxa_guard_abort
__cxa_guard_abort:
    push rbp
    mov rbp, rsp
    
    mov rax, rdi
    
    mov byte [rax], 0
    
    pop rbp
    ret

_main:
    call initConstructors ; initialize C++ constructors
    
    call _kinit

    cli

.halt:
    hlt
    jmp .halt