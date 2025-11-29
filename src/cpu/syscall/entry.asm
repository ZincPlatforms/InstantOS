global syscallEntry
global kernelStackTop
global userRSP
extern syscallHandler

extern saveSyscallState

syscallEntry:
    cli
    
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax
    
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    
    lea rdi, [rsp + 48]
    call saveSyscallState

    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    mov rdi, rax
    mov rsi, rbx
    mov r10, rcx
    mov rcx, rdx
    mov rdx, r10
    xor r8, r8
    xor r9, r9
    
    call syscallHandler
    
    mov [rsp], rax
    
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    
    sti

    iretq

section .data
align 8
userRSP: dq 0
kernelStackTop: dq 0