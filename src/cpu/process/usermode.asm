global enterUsermode

enterUsermode:
    cli
    
    mov rcx, rdi
    mov r11, rsi
    
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push 0x23
    push r11
    pushfq
    pop rax
    or rax, 0x200
    push rax
    push 0x1B
    push rcx
    
    iretq
