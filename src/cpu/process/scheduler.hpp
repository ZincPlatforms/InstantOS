#pragma once

#include "process.hpp"
#include <cpu/idt/interrupt.hpp>
#include <cstdint>

class Scheduler {
public:
    Scheduler() : currentProcess(nullptr), processListHead(nullptr), nextPID(1), initialized(false) {}
    
    static Scheduler& get();
    
    void initialize();
    void addProcess(Process* proc);
    void removeProcess(uint32_t pid);
    
    Process* getCurrentProcess() { return currentProcess; }
    void setCurrentProcess(Process* proc) { currentProcess = proc; }
    Process* getNextProcess();
    Process* getProcessByPID(uint32_t pid);
    
    void schedule();
    void schedule(struct InterruptFrame* frame);
    void scheduleFromSyscall();
    void yield();
    
    uint32_t allocatePID();
    
private:
    Process* currentProcess;
    Process* processListHead;
    uint32_t nextPID;
    bool initialized;
};

extern "C" void switchContext(ProcessContext* oldCtx, ProcessContext* newCtx);
