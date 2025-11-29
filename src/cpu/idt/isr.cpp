#include "interrupt.hpp"
#include "isr.hpp"
#include <limine.h>
#include <graphics/console.hpp>
#include <cpu/process/scheduler.hpp>

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

    if (frame->cs == 0x1B) {
        Process* current = Scheduler::get().getCurrentProcess();
        
        if (console && current) {
            console->drawText("Process ");
            console->drawNumber(current->getPID());
            console->drawText(" crashed.\n");
            const char* exception_name = (frame->interrupt < 32) ? exception_names[frame->interrupt] : "Unknown Exception";
            console->drawText(exception_name);
            console->drawText(" at RIP=");
            console->drawHex(frame->rip);
            console->drawText("\n");
        }
        
        if (current) {
            if (frame->interrupt == 0x0E) {
                current->sendSignal(SIGSEGV);
            } else {
                current->sendSignal(SIGTERM);
            }
            
            current->handlePendingSignals();
            
            if (current->getState() == ProcessState::Terminated) {
                Scheduler::get().schedule(frame);
            }
        }
        return;
    }

    if (console != nullptr) {
        fb->clear(0x000000);
        const char* exception_name = (frame->interrupt < 32) ? exception_names[frame->interrupt] : "Unknown";
        console->drawText(exception_name);
        console->drawText("\n\t- Interrupt: ");
        console->drawNumber(frame->interrupt);
        console->drawText("\n\t- Error Code: ");
        console->drawNumber(frame->errCode);
        console->drawText("\n\t- RIP: ");
        console->drawHex(frame->rip);
        console->drawText("\n\t- RSP: ");
        console->drawHex(frame->rsp);
        console->drawText("\n\t- CS: ");
        console->drawHex(frame->cs);
        
        if (frame->interrupt == 0x0E) {
            uint64_t cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
            console->drawText("\n\t- CR2: ");
            console->drawHex(cr2);
        }
        
        console->drawText("\n\t- RAX: ");
        console->drawHex(frame->rax);
        console->drawText("\n\t- RBP: ");
        console->drawHex(frame->rbp);
    }

    while(1);
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