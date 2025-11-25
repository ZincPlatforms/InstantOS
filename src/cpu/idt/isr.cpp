#include "interrupt.hpp"
#include "isr.hpp"
#include <limine.h>
#include <graphics/console.hpp>

Interrupt *interruptHandlers[256] = {nullptr};

extern Framebuffer* fb;
extern Console* console;

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

    if (console != nullptr) {
        fb->clear(0x000000);
        const char* exception_name = (frame->interrupt < 32) ? exception_names[frame->interrupt] : "Unknown";
        console->drawText(exception_name);
        console->drawText("\nInterrupt: ");
        console->drawHex(frame->interrupt);
        console->drawText("\nError Code: ");
        console->drawHex(frame->errCode);
        console->drawText("\nRIP: ");
        console->drawHex(frame->rip);
        console->drawText("\nRSP: ");
        console->drawHex(frame->rsp);
        
        if (frame->interrupt == 0x0E) {
            uint64_t cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
            console->drawText("CR2: ");
            console->drawHex(cr2);
        }
    }

    while (1);;
}
Interrupt::~Interrupt() = default;

void ISR::registerIRQ(uint8_t vector, Interrupt* handler) {
    interruptHandlers[vector] = handler;
    handler->initialize();
}

extern "C" void irqHandler(InterruptFrame* frame) {
    if (frame == nullptr) {
        LAPIC::get().sendEOI();
        return;
    }
    
    Interrupt* handler = interruptHandlers[frame->interrupt];
    if (handler != nullptr) {
        handler->Run(frame);
    } else {
        LAPIC::get().sendEOI();
    }
}