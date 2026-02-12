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
    
    Syscall::get().setKernelStack(nextProcess->getKernelStack());
    
    if (oldProcess && oldProcess->getState() == ProcessState::Running) {
        oldProcess->setState(ProcessState::Ready);
    }
    nextProcess->setState(ProcessState::Running);
    
    currentProcess = nextProcess;
    
    asm volatile("cli");
    
    if (oldProcess) {
        switchContext(oldProcess->getContext(), nextProcess->getContext());
    } else {
        switchContext(nullptr, nextProcess->getContext());
    }
    
    
    asm volatile("sti");
    
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
    
    if (oldProcess && frame->cs == 0x1B) {
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
        
        oldProcess->setValidUserState(true);
        
        if (oldProcess->getState() == ProcessState::Running) {
            oldProcess->setState(ProcessState::Ready);
        }
    } else if (oldProcess) {
        if (oldProcess->getState() == ProcessState::Running) {
            oldProcess->setState(ProcessState::Ready);
        }
    }
    
    uint64_t trampolineAddr = reinterpret_cast<uint64_t>(&processTrampoline);
    
    if (frame->cs != 0x1B) {
        if (oldProcess) {
            oldProcess->setState(ProcessState::Running);
            currentProcess = oldProcess;
        }
        
        return;
    }
    
    if (!nextProcess->hasValidUserState()) {
        if (oldProcess) {
            oldProcess->setState(ProcessState::Running);
            currentProcess = oldProcess;
        }
        
        return;
    }
    
    if (nextProcess->hasValidUserState()) {
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
        
        Syscall::get().setKernelStack(nextProcess->getKernelStack());
        
        uint64_t newCR3 = nextProcess->getContext()->cr3;
        asm volatile("mov %0, %%cr3" :: "r"(newCR3) : "memory");
        
        nextProcess->setState(ProcessState::Running);
        currentProcess = nextProcess;
    } else {
        Syscall::get().setKernelStack(nextProcess->getKernelStack());
        
        nextProcess->setState(ProcessState::Running);
        currentProcess = nextProcess;
        
        asm volatile("cli");
        
        if (oldProcess) {
            switchContext(oldProcess->getContext(), nextProcess->getContext());
        } else {
            switchContext(nullptr, nextProcess->getContext());
        }
        
        asm volatile("sti");
    }
    
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
    
    if (oldProcess && oldProcess->getState() == ProcessState::Running) {
        oldProcess->setState(ProcessState::Ready);
    }
    
    Syscall::get().setKernelStack(nextProcess->getKernelStack());
    
    nextProcess->setState(ProcessState::Running);
    currentProcess = nextProcess;
    
    asm volatile("cli");
    
    switchContext(nullptr, nextProcess->getContext());
    
    asm volatile("sti");
    
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
