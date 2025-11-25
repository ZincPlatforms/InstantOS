#pragma once

#include <cstdint>
#include "../mm/vmm.hpp"

enum class ProcessState {
    Ready,
    Running,
    Blocked,
    Terminated
};

struct alignas(16) FPUState {
    uint8_t data[512];
};

struct ProcessContext {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
    uint64_t cr3;
    uint64_t fpuStatePtr;
};

class Process {
public:
    Process(uint32_t pid);
    ~Process();
    
    uint32_t getPID() const { return pid; }
    ProcessState getState() const { return state; }
    void setState(ProcessState s) { state = s; }
    
    ProcessContext* getContext() { return &context; }
    FPUState* getFPUState() { return fpuState; }
    VMM* getVMM() { return &vmm; }
    
    uint64_t getKernelStack() const { return kernelStack; }
    uint64_t getUserStack() const { return userStack; }
    
    void setKernelStack(uint64_t stack) { kernelStack = stack; }
    void setUserStack(uint64_t stack) { userStack = stack; }
    
    Process* next;
    
private:
    uint32_t pid;
    ProcessState state;
    uint64_t kernelStack;
    uint64_t userStack;
    ProcessContext context;
    FPUState* fpuState;
    VMM vmm;
};
