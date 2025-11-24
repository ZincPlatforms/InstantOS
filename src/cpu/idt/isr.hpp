#pragma once

#include <cstdint>

struct InterruptFrame {
    uint64_t rcx, rdx, rax, rdi, rsi, r8, r9, r10, r11;
    uint64_t interrupt, errCode;
    uint64_t rip, cs, rflags, rsp;
} __attribute__((packed));

extern "C" void exceptionHandler(InterruptFrame* frame);
extern "C" void irqHandler(InterruptFrame* frame);

void registerInterruptHandler(uint8_t interrupt, void (*handler) (InterruptFrame* frame));