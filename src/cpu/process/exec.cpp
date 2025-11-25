#include "exec.hpp"
#include "../mm/pmm.hpp"

Process* ProcessExecutor::createKernelProcess(void (*entry)()) {
    uint32_t pid = Scheduler::get().allocatePID();
    Process* proc = new Process(pid);
    
    uint64_t stack = proc->getKernelStack();
    stack &= ~0xFULL;
    stack -= 8;
    
    proc->getContext()->rip = reinterpret_cast<uint64_t>(entry);
    proc->getContext()->rsp = stack;
    proc->getContext()->rbp = 0;
    proc->getContext()->rflags = 0x202;
    
    return proc;
}

Process* ProcessExecutor::createUserProcess(void* entry, void* userStack) {
    uint32_t pid = Scheduler::get().allocatePID();
    Process* proc = new Process(pid);
    
    uint64_t stack = reinterpret_cast<uint64_t>(userStack);
    stack &= ~0xFULL;
    stack -= 8;
    
    proc->setUserStack(stack);
    
    proc->getContext()->rip = reinterpret_cast<uint64_t>(entry);
    proc->getContext()->rsp = stack;
    proc->getContext()->rbp = 0;
    proc->getContext()->rflags = 0x202;
    
    return proc;
}
