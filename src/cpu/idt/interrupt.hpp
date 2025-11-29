#pragma once
#include <cpu/apic/lapic.hpp>

struct InterruptFrame {
    // Pushed by pushad
    uint64_t r11, r10, r9, r8, rbp, rsi, rdi, rax, rcx, rdx, rbx;
    
    // Pushed by ISR
    uint64_t interrupt, errCode;
    
    // Pushed by CPU
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));

class Interrupt {
public:
    Interrupt() = default;
    virtual ~Interrupt();

    virtual void initialize() = 0;

    virtual void Run(InterruptFrame* frame) = 0;
    void sendEOI(){
        LAPIC::get().sendEOI();
    }
};