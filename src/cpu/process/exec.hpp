#pragma once

#include "process.hpp"
#include "scheduler.hpp"

class ProcessExecutor {
public:
    static Process* createKernelProcess(void (*entry)());
    static Process* createUserProcess(void* entry, void* userStack);
    
private:
    static void kernelProcessWrapper();
};
