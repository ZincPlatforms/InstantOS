#pragma once

#include <cstdint>
#include <cpu/mm/vmm.hpp>

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
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, rflags, cr3, fxstate;
};

#define NSIG 32
#define SIGKILL 9
#define SIGSEGV 11
#define SIGTERM 15

typedef void (*sighandler_t)(int);

struct SignalHandler {
    sighandler_t handlers[NSIG];
    uint64_t pending;
    uint64_t blocked;
};


class GDT;

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
    
    void jumpToUsermode(uint64_t entry, GDT* gdt);
    
    uint32_t getParentPID() const { return parentPID; }
    void setParentPID(uint32_t ppid) { parentPID = ppid; }
    
    int getExitCode() const { return exitCode; }
    void setExitCode(int code) { exitCode = code; }
    
    Process* next;
    
    bool hasValidUserState() const { return validUserState; }
    void setValidUserState(bool valid) { validUserState = valid; }
    
    SignalHandler* getSignalHandler() { return &signalHandler; }
    void sendSignal(int sig);
    void handlePendingSignals();
    
private:
    uint32_t pid;
    uint32_t parentPID;
    int exitCode;
    ProcessState state;
    uint64_t kernelStack;
    uint64_t userStack;
    ProcessContext context;
    FPUState* fpuState;
    VMM vmm;
    bool validUserState;
    SignalHandler signalHandler;
};
