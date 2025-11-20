#include "isr.hpp"
#include <limine.h>

void (*interruptHandlers[256]) (InterruptFrame* frame) = {nullptr};

extern "C" void exceptionHandler(InterruptFrame* frame) {
    const char* exception_names[] = {
        "Division By Zero", "Debug", "Non-Maskable Interrupt", "Breakpoint",
        "Overflow", "Bound Range Exceeded", "Unknown Interrupt", "Device Not Available",
        "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
        "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
        "x87 Floating-Point", "Alignment Check", "Machine Check", "SIMD Floating-Point",
        "Virtualization", "Control Protection", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor Injection", "VMM Communication", "Security", "Reserved"
    };

    const char* exception_name = (frame->interrupt < 32) ? exception_names[frame->interrupt] : "Unknown";

    switch (frame->interrupt) {
        case 0x12: // machine fail

        break;

        case 0x0E: // Page Fault
        
        break;

        case 0x0D: // GPF
    
        break;

        case 0x08: // Double Fault

        break;

        case 0x06: // Unknown Instruction

        break;
    }

    while (1);;
}


void registerInterruptHandler(uint8_t interrupt, void (*handler) (InterruptFrame* frame)) {
    interruptHandlers[interrupt] = handler;
}

extern "C" void irqHandler(InterruptFrame* frame) {
    if (&interruptHandlers[frame->interrupt] != nullptr) {
        interruptHandlers[frame->interrupt](frame);
    }
}