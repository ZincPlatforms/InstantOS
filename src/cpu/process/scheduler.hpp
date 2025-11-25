#pragma once

#include "process.hpp"
#include <cstdint>

class Scheduler {
public:
    Scheduler() : currentProcess(nullptr), processListHead(nullptr), nextPID(1), initialized(false) {}
    
    static Scheduler& get();
    
    void initialize();
    void addProcess(Process* proc);
    void removeProcess(uint32_t pid);
    
    Process* getCurrentProcess() { return currentProcess; }
    Process* getNextProcess();
    
    void schedule();
    void yield();
    
    uint32_t allocatePID();
    
private:
    Process* currentProcess;
    Process* processListHead;
    uint32_t nextPID;
    bool initialized;
};

extern "C" void switchContext(ProcessContext* oldCtx, ProcessContext* newCtx);
