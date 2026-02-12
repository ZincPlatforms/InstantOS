#pragma once

#include <cstdint>

struct OSInfo {
    char osname[10];    char loggedOnUser[32];
    char cpuname[64];
    char maxRamGB[8];
    char usedRamGB[8];
};

enum class SyscallNumber : uint64_t {
    OSInfo,
    ProcInfo,
    Exit,
    Write,
    Read,
    Open,
    Close,
    GetPID,
    Fork,
    Exec,
    Wait,
    Kill,
    Mmap,
    Munmap,
    Yield,
    Sleep,
    GetTime,
    Clear,
    FBInfo,
    FBMap,
    Signal,
    SigReturn
};

struct SyscallFrame {
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
} __attribute__((packed));

class Syscall {
public:
    Syscall() : initialized(false) {}
    
    static Syscall& get();
    
    void initialize();
    void setKernelStack(uint64_t stack);
    uint64_t handle(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);
    
private:
    bool initialized;
    
    uint64_t sys_exit(uint64_t code);
    uint64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count);
    uint64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count);
    uint64_t sys_open(uint64_t path, uint64_t flags, uint64_t mode);
    uint64_t sys_close(uint64_t fd);
    uint64_t sys_getpid();
    uint64_t sys_fork();
    uint64_t sys_exec(uint64_t path, uint64_t argv, uint64_t envp);
    uint64_t sys_wait(uint64_t pid, uint64_t status);
    uint64_t sys_kill(uint64_t pid, uint64_t sig);
    uint64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot);
    uint64_t sys_munmap(uint64_t addr, uint64_t length);
    uint64_t sys_yield();
    uint64_t sys_sleep(uint64_t ms);
    uint64_t sys_gettime();
    uint64_t sys_clear();
    uint64_t sys_fb_info(uint64_t info_ptr);
    uint64_t sys_fb_map();
    uint64_t sys_signal(uint64_t sig, uint64_t handler);
    uint64_t sys_sigreturn();
    uint64_t sys_osinfo(uint64_t info_ptr);
};

extern "C" void syscallEntry();
extern "C" uint64_t syscallHandler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);
