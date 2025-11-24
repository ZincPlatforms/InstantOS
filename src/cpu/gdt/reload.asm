[bits 64]

; void reloadSegments(uint16_t data, uint16_t code);
global reloadSegments
reloadSegments:
    mov ds, di
    mov es, di
    mov fs, di
    mov gs, di
    mov ss, di

    push rsi
    push qword .after

    retfq

.after:
    ret