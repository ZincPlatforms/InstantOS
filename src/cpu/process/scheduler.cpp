#include "scheduler.hpp"
#include <cpu/gdt/gdt.hpp>
#include <graphics/console.hpp>
#include <cpu/syscall/syscall.hpp>
#include <cpu/idt/interrupt.hpp>

extern GDT* globalGDT;

Scheduler schedulerInstance;

Scheduler& Scheduler::get() {
    return schedulerInstance;
}

void Scheduler::initialize() {
    if (initialized) return;
    
    currentProcess = nullptr;
    processListHead = nullptr;
    nextPID = 1;
    initialized = true;
}

void Scheduler::addProcess(Process* proc) {
    if (!proc) return;
    
    proc->next = nullptr;
    
    if (!processListHead) {
        processListHead = proc;
    } else {
        Process* current = processListHead;
        while (current->next) {
            current = current->next;
        }
        current->next = proc;
    }
    
}

void Scheduler::removeProcess(uint32_t pid) {
    if (!processListHead) return;
    
    Process* toDelete = nullptr;
    
    if (processListHead->getPID() == pid) {
        toDelete = processListHead;
        processListHead = processListHead->next;
    } else {
        Process* current = processListHead;
        while (current->next) {
            if (current->next->getPID() == pid) {
                toDelete = current->next;
                current->next = current->next->next;
                break;
            }
            current = current->next;
        }
    }
    
    if (toDelete) {
        if (currentProcess == toDelete) currentProcess = nullptr;
        delete toDelete;
    }
}

Process* Scheduler::getNextProcess() {
    if (!currentProcess) {
        return processListHead;
    }
    
    Process* next = currentProcess->next;
    if (!next) {
        next = processListHead;
    }
    
    int iterations = 0;
    while (next && next->getState() != ProcessState::Ready && next->getState() != ProcessState::Running) {
        iterations++;
        next = next->next;
        if (!next) {
            next = processListHead;
        }
        if (next == currentProcess) {
            return nullptr;
        }
    }
    
    return next;
}

void Scheduler::schedule() {
    if (!initialized || !processListHead) return;
    
    if (currentProcess) {
        currentProcess->handlePendingSignals();
    }
    
    Process* nextProcess = getNextProcess();
    if (!nextProcess || nextProcess == currentProcess) {
        return;
    }
        
    Process* oldProcess = currentProcess;
    
    // Update kernel stack BEFORE switching
    Syscall::get().setKernelStack(nextProcess->getKernelStack());
    
    // Update state
    if (oldProcess && oldProcess->getState() == ProcessState::Running) {
        oldProcess->setState(ProcessState::Ready);
    }
    nextProcess->setState(ProcessState::Running);
    
    // Update currentProcess BEFORE the switch
    // This is critical - we update it before switching so when we return
    // from the context switch, currentProcess is already correct
    currentProcess = nextProcess;
    
    // Disable interrupts during context switch
    asm volatile("cli");
    
    // Perform the actual context switch
    // We always save the old context, even if terminated, because we need to
    // properly return from the interrupt handler
    if (oldProcess) {
        switchContext(oldProcess->getContext(), nextProcess->getContext());
    } else {
        switchContext(nullptr, nextProcess->getContext());
    }
    
    // When we return here, we're running as the "new" process
    // currentProcess should already be correct
    
    // Re-enable interrupts after switch
    asm volatile("sti");
    
    // Clean up terminated processes AFTER switching away from them
    // We check oldProcess here because we've already switched to the new process
    if (oldProcess && oldProcess->getState() == ProcessState::Terminated) {
        removeProcess(oldProcess->getPID());
    }
}
extern "C" void processTrampoline();

void Scheduler::schedule(InterruptFrame* frame) {
    if (!initialized || !processListHead || !frame) return;
    
    Process* nextProcess = getNextProcess();
    if (!nextProcess || nextProcess == currentProcess) {
        return;
    }
        
    Process* oldProcess = currentProcess;
    
    // Save current process state from interrupt frame
    // BUT only if we interrupted usermode (CS = 0x1B)
    // If we interrupted kernel mode (CS = 0x08), don't save - the process
    // context still has the correct user-mode state from before the syscall
    if (oldProcess && frame->cs == 0x1B) {
        // The interrupt frame contains the user-mode state that was interrupted
        // Save it into the process context
        oldProcess->getContext()->rip = frame->rip;
        oldProcess->getContext()->rsp = frame->rsp;
        oldProcess->getContext()->rflags = frame->rflags;
        oldProcess->getContext()->rax = frame->rax;
        oldProcess->getContext()->rbx = frame->rbx;
        oldProcess->getContext()->rcx = frame->rcx;
        oldProcess->getContext()->rdx = frame->rdx;
        oldProcess->getContext()->rsi = frame->rsi;
        oldProcess->getContext()->rdi = frame->rdi;
        oldProcess->getContext()->rbp = frame->rbp;
        oldProcess->getContext()->r8 = frame->r8;
        oldProcess->getContext()->r9 = frame->r9;
        oldProcess->getContext()->r10 = frame->r10;
        oldProcess->getContext()->r11 = frame->r11;
        // r12-r15 are NOT in InterruptFrame - don't try to save them
        
        // Mark that this process now has valid saved user state
        oldProcess->setValidUserState(true);
        
        if (oldProcess->getState() == ProcessState::Running) {
            oldProcess->setState(ProcessState::Ready);
        }
    } else if (oldProcess) {
        // We interrupted kernel mode - just mark as ready
        if (oldProcess->getState() == ProcessState::Running) {
            oldProcess->setState(ProcessState::Ready);
        }
    }
    
    // Check if next process has ever run in usermode
    // If RIP points to processTrampoline, this is the first time
    uint64_t trampolineAddr = reinterpret_cast<uint64_t>(&processTrampoline);
    
    // Check if we interrupted usermode or kernel mode
    // If CS != 0x1B, we're in kernel mode and can't do a context switch via interrupt frame
    if (frame->cs != 0x1B) {
        // Don't switch - keep running current process
        if (oldProcess) {
            oldProcess->setState(ProcessState::Running);
            currentProcess = oldProcess;
        }
        
        return;
    }
    
    // From an interrupt handler, we can ONLY schedule processes with valid user state
    // We cannot use switchContext because it would save kernel-mode state
    if (!nextProcess->hasValidUserState()) {
        // Don't switch - keep running old process
        if (oldProcess) {
            oldProcess->setState(ProcessState::Running);
            currentProcess = oldProcess;
        }
        
        return;
    }
    
    // Process has valid user state - load into interrupt frame
    if (nextProcess->hasValidUserState()) {
        // Process has run before and has valid saved state - load into interrupt frame
        frame->rip = nextProcess->getContext()->rip;
        frame->rsp = nextProcess->getContext()->rsp;
        frame->rflags = nextProcess->getContext()->rflags;
        frame->rax = nextProcess->getContext()->rax;
        frame->rbx = nextProcess->getContext()->rbx;
        frame->rcx = nextProcess->getContext()->rcx;
        frame->rdx = nextProcess->getContext()->rdx;
        frame->rsi = nextProcess->getContext()->rsi;
        frame->rdi = nextProcess->getContext()->rdi;
        frame->rbp = nextProcess->getContext()->rbp;
        frame->r8 = nextProcess->getContext()->r8;
        frame->r9 = nextProcess->getContext()->r9;
        frame->r10 = nextProcess->getContext()->r10;
        frame->r11 = nextProcess->getContext()->r11;
        // r12-r15 are NOT in InterruptFrame, they're callee-saved
        
        // Update kernel stack
        Syscall::get().setKernelStack(nextProcess->getKernelStack());
        
        // Switch page tables
        uint64_t newCR3 = nextProcess->getContext()->cr3;
        asm volatile("mov %0, %%cr3" :: "r"(newCR3) : "memory");
        
        // Update state
        nextProcess->setState(ProcessState::Running);
        currentProcess = nextProcess;
    } else {
        // Update kernel stack
        Syscall::get().setKernelStack(nextProcess->getKernelStack());
        
        // Update state
        nextProcess->setState(ProcessState::Running);
        currentProcess = nextProcess;
        
        // Disable interrupts during context switch
        asm volatile("cli");
        
        // Perform context switch
        if (oldProcess) {
            switchContext(oldProcess->getContext(), nextProcess->getContext());
        } else {
            switchContext(nullptr, nextProcess->getContext());
        }
        
        // Re-enable interrupts after switch
        asm volatile("sti");
    }
    
    // Clean up terminated processes - DISABLED: causes crashes when called from interrupt
    // TODO: Implement deferred cleanup or fix removeProcess to be interrupt-safe
    // if (oldProcess && oldProcess->getState() == ProcessState::Terminated) {
    //     removeProcess(oldProcess->getPID());
    // }
}

void Scheduler::yield() {
    schedule();
}

void Scheduler::scheduleFromSyscall() {
    if (!initialized || !processListHead) return;
    
    Process* nextProcess = getNextProcess();
    if (!nextProcess || nextProcess == currentProcess) {
        return;
    }
    
    Process* oldProcess = currentProcess;
    
    // DON'T save old context - we already saved user state on syscall entry
    // Just mark as ready
    if (oldProcess && oldProcess->getState() == ProcessState::Running) {
        oldProcess->setState(ProcessState::Ready);
    }
    
    // Update kernel stack
    Syscall::get().setKernelStack(nextProcess->getKernelStack());
    
    // Update state
    nextProcess->setState(ProcessState::Running);
    currentProcess = nextProcess;
    
    // Disable interrupts during context switch
    asm volatile("cli");
    
    // Switch to new process WITHOUT saving old context
    // Pass nullptr for old context
    switchContext(nullptr, nextProcess->getContext());
    
    // Re-enable interrupts after switch
    asm volatile("sti");
    
    // Clean up terminated processes
    if (oldProcess && oldProcess->getState() == ProcessState::Terminated) {
        removeProcess(oldProcess->getPID());
    }
}

uint32_t Scheduler::allocatePID() {
    return nextPID++;
}

Process* Scheduler::getProcessByPID(uint32_t pid) {
    Process* current = processListHead;
    while (current) {
        if (current->getPID() == pid) {
            return current;
        }
        current = current->next;
    }
    return nullptr;
}
