global switchContext
global processTrampoline

switchContext:
    cmp rdi, 0
    je .load_new

    mov [rdi + 0],  rax
    mov [rdi + 8],  rbx
    mov [rdi + 16], rcx
    mov [rdi + 24], rdx
    mov [rdi + 32], rsi
    mov [rdi + 40], rdi
    mov [rdi + 48], rbp
    
    mov [rdi + 56], rsp
    
    mov [rdi + 64], r8
    mov [rdi + 72], r9
    mov [rdi + 80], r10
    mov [rdi + 88], r11
    mov [rdi + 96], r12
    mov [rdi + 104], r13
    mov [rdi + 112], r14
    mov [rdi + 120], r15

    mov rax, [rsp]
    mov [rdi + 128], rax

    pushfq
    pop rax
    mov [rdi + 136], rax

    mov rax, cr3
    mov [rdi + 144], rax

    mov rax, [rdi + 152]
    test rax, rax
    jz .load_new
    fxsave [rax]

.load_new:
    mov r12, [rsi + 144]
    mov r13, [rsi + 136]
    mov r14, [rsi + 128]
    mov r15, [rsi + 56]
    
    mov rax, [rsi + 152]
    test rax, rax
    jz .skip_fpu
    fxrstor [rax]
.skip_fpu:

    mov cr3, r12

    mov rax, r13
    and rax, ~0x200
    push rax
    popfq
    
    mov rsp, r15
    
    push r14
    
    mov rax, [rsi + 0]
    mov rbx, [rsi + 8]
    mov rcx, [rsi + 16]
    mov rdx, [rsi + 24]
    mov rdi, [rsi + 40]
    mov rbp, [rsi + 48]
    mov r8,  [rsi + 64]
    mov r9,  [rsi + 72]
    mov r10, [rsi + 80]
    mov r11, [rsi + 88]
    mov r12, [rsi + 96]
    mov r13, [rsi + 104]
    mov r14, [rsi + 112]
    mov r15, [rsi + 120]
    mov rsi, [rsi + 32]
    
    ret

processTrampoline:
    cli
    
    mov rax, [rsp + 0]
    mov rbx, [rsp + 8]
    add rsp, 16
    
    push 0x23
    push rbx
    pushfq
    pop rcx
    or rcx, 0x200
    push rcx
    push 0x1B
    push rax
        
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    
    iretq