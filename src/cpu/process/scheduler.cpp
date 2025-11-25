#include "scheduler.hpp"
#include "../gdt/gdt.hpp"

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
    
    if (processListHead->getPID() == pid) {
        Process* toDelete = processListHead;
        processListHead = processListHead->next;
        delete toDelete;
        return;
    }
    
    Process* current = processListHead;
    while (current->next) {
        if (current->next->getPID() == pid) {
            Process* toDelete = current->next;
            current->next = current->next->next;
            delete toDelete;
            return;
        }
        current = current->next;
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
    
    while (next && next->getState() != ProcessState::Ready && next->getState() != ProcessState::Running) {
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
    
    Process* nextProcess = getNextProcess();
    if (!nextProcess || nextProcess == currentProcess) {
        return;
    }
    
    Process* oldProcess = currentProcess;
    currentProcess = nextProcess;
    currentProcess->setState(ProcessState::Running);
    
    if (oldProcess && oldProcess->getState() == ProcessState::Running) {
        oldProcess->setState(ProcessState::Ready);
    }
    
    if (oldProcess) {
        switchContext(oldProcess->getContext(), currentProcess->getContext());
    } else {
        switchContext(nullptr, currentProcess->getContext());
    }
}

void Scheduler::yield() {
    schedule();
}

uint32_t Scheduler::allocatePID() {
    return nextPID++;
}
