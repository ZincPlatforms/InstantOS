#include "syscall.hpp"
#include <cpu/gdt/gdt.hpp>
#include <cpu/process/scheduler.hpp>
#include <cpu/process/exec.hpp>
#include <fs/vfs/vfs.hpp>
#include <graphics/console.hpp>
#include <interrupts/keyboard.hpp>
#include <interrupts/timer.hpp>
#include <x86_64/requests.hpp>
#include <x86_64/ports.hpp>
#include <string.h>
#include <cpuid.h>
#include <string.h>

extern Console* console;
extern Keyboard* globalKeyboard;
extern Timer* globalTimer;
extern "C" uint64_t kernelStackTop;

Syscall syscallInstance;

bool hasSyscall(){
    uint32_t eax = 0x80000001, ebx = 0, ecx = 0, edx = 0;
    cpuid(&eax, &ebx, &ecx, &edx);
    return (edx >> 11) & 1;
}

Syscall& Syscall::get() {
    return syscallInstance;
}

void Syscall::initialize() {
    if (initialized) return;
    initialized = true;

    if(!hasSyscall()){
        console->drawText("No 'syscall' instruction support. halting...");
        asm volatile("cli");
        while(1);
    }    
}

void Syscall::setKernelStack(uint64_t stack) {
    kernelStackTop = stack;
}

uint64_t Syscall::handle(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch ((SyscallNumber)syscall_num) {
        using enum SyscallNumber;
        case OSInfo:
            return sys_osinfo(arg1);
        case ProcInfo:
            return 0;        case Exit:
            return sys_exit(arg1);
        case Write:
            return sys_write(arg1, arg2, arg3);
        case Read:
            return sys_read(arg1, arg2, arg3);
        case Open:
            return sys_open(arg1, arg2, arg3);
        case Close:
            return sys_close(arg1);
        case GetPID:
            return sys_getpid();
        case Fork:
            return sys_fork();
        case Exec:
            return sys_exec(arg1, arg2, arg3);
        case Wait:
            return sys_wait(arg1, arg2);
        case Kill:
            return sys_kill(arg1, arg2);
        case Mmap:
            return sys_mmap(arg1, arg2, arg3);
        case Munmap:
            return sys_munmap(arg1, arg2);
        case Yield:
            return sys_yield();
        case Sleep:
            return sys_sleep(arg1);
        case GetTime:
            return sys_gettime();
        case Clear:
            return sys_clear();
        case FBInfo:
            return sys_fb_info(arg1);
        case FBMap:
            return sys_fb_map();
        case Signal:
            return sys_signal(arg1, arg2);
        case SigReturn:
            return sys_sigreturn();
        default:
            return (uint64_t)-1;
    }
}

uint64_t Syscall::sys_exit(uint64_t code) {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) {
        return (uint64_t)-1;
    }

    current->setExitCode((int)code);
    current->setState(ProcessState::Terminated);
    
    return 0;
}

static bool isValidUserPointer(uint64_t ptr, size_t size) {
    if (ptr >= 0xFFFF800000000000) {
        return false;
    }
    if (ptr == 0) {
        return false;
    }

    if (ptr + size < ptr) {
        return false;
    }
    if (ptr + size >= 0x0000800000000000) {
        return false;
    }
    return true;
}

uint64_t Syscall::sys_write(uint64_t fd, uint64_t buf, uint64_t count) {
    if (fd == 1 || fd == 2) {
        if (!console || count == 0) {
            return count;
        }
        
        if (!isValidUserPointer(buf, count)) return -1;
        
        const char* str = reinterpret_cast<const char*>(buf);
        
        for (size_t i = 0; i < count; i++) {
            char temp[2] = { str[i], '\0' };
            console->drawText(temp);
        }
        
        return count;
    }
    
    return -1;
}

uint64_t Syscall::sys_read(uint64_t fd, uint64_t buf, uint64_t count) {
    if (fd == 0) {        
        if (!globalKeyboard) {
            if (console) console->drawText("No keyboard!\nPress F2 to continue...\n");
            return -1;
        }
        
        if (!isValidUserPointer(buf, count)) {
            return -1;
        }
        
        for (int i = 0; i < 100; i++) {
            if (globalKeyboard->poll() == 0) break;
        }
        
        char* buffer = reinterpret_cast<char*>(buf);
        size_t bytesRead = 0;
        
        asm volatile("sti");
        
        while (bytesRead < count) {
            char c = globalKeyboard->poll();
            
            if (c == 0) {
                asm volatile("pause");
                continue;
            }
            
            if (c == '\b') {
                if (bytesRead > 0) {
                    bytesRead--;
                    if (console) {
                        console->drawText("\b");
                        console->drawText(" ");
                        console->drawText("\b");
                    }
                }
                continue;
            }
            
            buffer[bytesRead++] = c;
            
            if (console && (c == '\n' || (c >= 32 && c < 127))) {
                char text[2] = {c, '\0'};
                console->drawText(text);
            }
            
            if (c == '\n') {
                break;
            }
        }
        
        asm volatile("cli");
        
        return bytesRead;
    }
    
    return -1;
}

uint64_t Syscall::sys_open(uint64_t path, uint64_t flags, uint64_t mode) {
    return -1;
}

uint64_t Syscall::sys_close(uint64_t fd) {
    return -1;
}

uint64_t Syscall::sys_getpid() {
    Process* current = Scheduler::get().getCurrentProcess();
    return current ? current->getPID() : 0;
}

uint64_t Syscall::sys_fork() {
    return -1;
}

uint64_t Syscall::sys_exec(uint64_t path, uint64_t argv, uint64_t envp __attribute__((unused))) {
    if (!isValidUserPointer(path, 1)) {
        return -1;
    }
    
    const char* userPathname = reinterpret_cast<const char*>(path);
    size_t pathLen = 0;
    while (userPathname[pathLen] && pathLen < 256) pathLen++;
    
    char* pathname = new char[pathLen + 1];
    memcpy(pathname, userPathname, pathLen);
    pathname[pathLen] = '\0';
    
    if (argv != 0 && !isValidUserPointer(argv, sizeof(char*))) {
        delete[] pathname;
        return -1;
    }
    
    const char** userArgv = reinterpret_cast<const char**>(argv);
    int argc = 0;
    
    if (userArgv) {
        while (userArgv[argc] != nullptr && argc < 64) {
            argc++;
        }
    }
    
    const char** kernelArgv = new const char*[argc + 1];
    for (int i = 0; i < argc; i++) {
        const char* userArg = userArgv[i];
        size_t argLen = 0;
        while (userArg[argLen] && argLen < 256) argLen++;
        
        char* kernelArg = new char[argLen + 1];
        memcpy(kernelArg, userArg, argLen);
        kernelArg[argLen] = '\0';
        kernelArgv[i] = kernelArg;
    }
    kernelArgv[argc] = nullptr;
    
    uint64_t userCR3;
    asm volatile("mov %%cr3, %0" : "=r"(userCR3));
    
    extern VMM vmm;
    vmm.load();
    
    Process* newProc = ProcessExecutor::loadUserBinaryWithArgs(pathname, argc, kernelArgv);
    
    asm volatile("mov %0, %%cr3" :: "r"(userCR3) : "memory");
    
    for (int i = 0; i < argc; i++) {
        delete[] kernelArgv[i];
    }
    delete[] kernelArgv;
    delete[] pathname;
    
    if (!newProc) {
        return -1;
    }
    
    Process* current = Scheduler::get().getCurrentProcess();
    if (current) {
        newProc->setParentPID(current->getPID());
    }
    
    Scheduler::get().addProcess(newProc);
    Scheduler::get().scheduleFromSyscall();
    
    return 0;
}

uint64_t Syscall::sys_wait(uint64_t pid, uint64_t statusPtr) {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) {
        return (uint64_t)-1;
    }
        
    Process* child = Scheduler::get().getProcessByPID((uint32_t)pid);
    if (!child) {
        return (uint64_t)-1;
    }
    
    if (child->getParentPID() != current->getPID()) {
        return (uint64_t)-1;
    }
    
    if (statusPtr) {
        int* status = reinterpret_cast<int*>(statusPtr);
        *status = 0;
    }
    
    return 0;
}

uint64_t Syscall::sys_kill(uint64_t pid, uint64_t sig) {
    Process* target = Scheduler::get().getProcessByPID((uint32_t)pid);
    if (!target) return -1;
    
    target->sendSignal((int)sig);
    return 0;
}

uint64_t Syscall::sys_mmap(uint64_t addr, uint64_t length, uint64_t prot) {
    return -1;
}

uint64_t Syscall::sys_munmap(uint64_t addr, uint64_t length) {
    return -1;
}

uint64_t Syscall::sys_yield() {
    Scheduler::get().yield();
    return 0;
}

uint64_t Syscall::sys_sleep(uint64_t ms) {
    if (!globalTimer) return -1;
    
    uint64_t start = globalTimer->getMilliseconds();
    uint64_t target = start + ms;
    
    asm volatile("sti");
    
    if (ms < 10) {
        while (globalTimer->getMilliseconds() < target) {
            asm volatile("pause");
        }
        asm volatile("cli");
        return 0;
    }
    
    uint64_t yieldCounter = 0;
    while (globalTimer->getMilliseconds() < target) {
        if (yieldCounter++ % 1000 == 0) {
            Scheduler::get().yield();
        }
        
        asm volatile("pause");
    }
    asm volatile("cli");
    return 0;
}

uint64_t Syscall::sys_gettime() {
    if (!globalTimer) {
        return 0;
    }
    return globalTimer->getMilliseconds();
}

uint64_t Syscall::sys_clear() {
    if (console) {
        console->drawText("\033[2J");
    }
    return 0;
}

struct FBInfo {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint16_t bpp;
    uint8_t redMaskSize;
    uint8_t redMaskShift;
    uint8_t greenMaskSize;
    uint8_t greenMaskShift;
    uint8_t blueMaskSize;
    uint8_t blueMaskShift;
};

uint64_t Syscall::sys_fb_info(uint64_t info_ptr) {
    if (!console) return (uint64_t)-1;
    
    extern Framebuffer* fb;
    if (!fb) return (uint64_t)-1;
    
    if (!isValidUserPointer(info_ptr, sizeof(FBInfo))) {
        return (uint64_t)-1;
    }
    
    uint64_t fb_kernel_virt = reinterpret_cast<uint64_t>(fb->getRaw());
    uint64_t fb_phys = fb_kernel_virt - hhdm_request.response->offset;
    
    constexpr uint64_t USER_FB_BASE = 0x0000700000000000;
    
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) return (uint64_t)-1;
    
    size_t fb_size_bytes = fb->getPitch() * fb->getHeight() * sizeof(uint32_t);
    size_t pages = (fb_size_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    
    current->getVMM()->mapRange(
        reinterpret_cast<void*>(USER_FB_BASE),
        reinterpret_cast<void*>(fb_phys),
        pages,
        PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_CACHE_DISABLE
    );

    
    FBInfo kernel_info;
    kernel_info.addr = USER_FB_BASE;
    kernel_info.width = fb->getWidth();
    kernel_info.height = fb->getHeight();
    kernel_info.pitch = fb->getPitch();
    
    kernel_info.redMaskSize = fb->getRedMaskSize();
    kernel_info.redMaskShift = fb->getRedMaskShift();
    kernel_info.greenMaskSize = fb->getGreenMaskSize();
    kernel_info.greenMaskShift = fb->getGreenMaskShift();
    kernel_info.blueMaskSize = fb->getBlueMaskSize();
    kernel_info.blueMaskShift = fb->getBlueMaskShift();
    
    kernel_info.bpp = sizeof(uint32_t);
    
    char* user_ptr = reinterpret_cast<char*>(info_ptr);
    memcpy(user_ptr, &kernel_info, sizeof(FBInfo));
    return 0;
}

uint64_t Syscall::sys_fb_map() {
    if (!console) return (uint64_t)-1;
    
    extern Framebuffer* fb;
    if (!fb) return (uint64_t)-1;
    
    return reinterpret_cast<uint64_t>(fb->getRaw());
}

extern "C" void saveSyscallState(uint64_t* stack) {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) return;
    
    uint64_t* regs = stack;
    uint64_t* cpuFrame = stack + 15;
    
    uint64_t user_rip = cpuFrame[0];
    uint64_t user_cs = cpuFrame[1];
    uint64_t user_rflags = cpuFrame[2];
    uint64_t user_rsp = cpuFrame[3];
    
    if (user_cs == 0x1B) {
        current->getContext()->rip = user_rip;
        current->getContext()->rsp = user_rsp;
        current->getContext()->rflags = user_rflags;
        
        current->getContext()->rax = regs[0];
        current->getContext()->rbx = regs[1];
        current->getContext()->rcx = regs[2];
        current->getContext()->rdx = regs[3];
        current->getContext()->rsi = regs[4];
        current->getContext()->rdi = regs[5];
        current->getContext()->rbp = regs[6];
        current->getContext()->r8 = regs[7];
        current->getContext()->r9 = regs[8];
        current->getContext()->r10 = regs[9];
        current->getContext()->r11 = regs[10];
        current->getContext()->r12 = regs[11];
        current->getContext()->r13 = regs[12];
        current->getContext()->r14 = regs[13];
        current->getContext()->r15 = regs[14];
        
        current->setValidUserState(true);
    }
}

extern "C" uint64_t syscallHandler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    return Syscall::get().handle(syscall_num, arg1, arg2, arg3, arg4, arg5);
}

uint64_t Syscall::sys_signal(uint64_t sig, uint64_t handler) {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) return (uint64_t)-1;
    
    if (sig >= NSIG) return (uint64_t)-1;
    
    SignalHandler* sh = current->getSignalHandler();
    uint64_t old = reinterpret_cast<uint64_t>(sh->handlers[sig]);
    sh->handlers[sig] = reinterpret_cast<sighandler_t>(handler);
    
    return old;
}

uint64_t Syscall::sys_sigreturn() {
    Process* current = Scheduler::get().getCurrentProcess();
    if (!current) return (uint64_t)-1;
    
    uint64_t* stack = reinterpret_cast<uint64_t*>(current->getContext()->rsp);
    current->getContext()->rip = stack[0];
    current->getContext()->rsp += 128;
    
    return 0;
}

size_t strncpyToUser(char* user_dest, const char* kernel_src, size_t max_len) {
    if (!isValidUserPointer((uint64_t)user_dest, max_len)) {
        return (size_t)-1;
    }

    if (max_len == 0) return 0;

    while (*kernel_src && (*kernel_src == ' ' || *kernel_src == '\t')) {
        kernel_src++;
    }

    size_t i = 0;
    size_t src_len = 0;
    
    const char* src_start = kernel_src;
    const char* src_end = kernel_src;
    while (*src_end) src_end++;
    
    while (src_end > src_start && (src_end[-1] == ' ' || src_end[-1] == '\t')) {
        src_end--;
    }
    
    src_len = src_end - src_start;
    
    for (i = 0; i < max_len - 1 && i < src_len; i++) {
        user_dest[i] = src_start[i];
    }
    
    user_dest[i] = '\0';
    return i;
}

size_t uitoa(uint64_t value, char* buffer, size_t buffer_size) {
    if (buffer_size == 0) return 0;

    char temp[20];    size_t i = 0;

    if (value == 0) {
        if (buffer_size > 1) {
            buffer[0] = '0';
            buffer[1] = '\0';
            return 1;
        } else {
            buffer[0] = '\0';
            return 0;
        }
    }

    while (value && i < sizeof(temp)) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }

    size_t j = 0;
    while (i > 0 && j < buffer_size - 1) {
        buffer[j++] = temp[--i];
    }

    buffer[j] = '\0';
    return j;
}


uint64_t Syscall::sys_osinfo(uint64_t info_ptr) {
    if (!isValidUserPointer(info_ptr, sizeof(OSInfo))) {
        return (uint64_t)-1;
    }
    
    OSInfo* info = reinterpret_cast<OSInfo*>(info_ptr);

    memset(info, 0, sizeof(OSInfo));

    unsigned int eax, ebx, ecx, edx;
    char vendor[13];
    __get_cpuid(0, &eax, &ebx, &ecx, &edx);

    *((unsigned int*)&vendor[0]) = ebx;
    *((unsigned int*)&vendor[4]) = edx;
    *((unsigned int*)&vendor[8]) = ecx;
    vendor[12] = '\0';

    char brand[49];    memset(brand, 0, sizeof(brand));    
    __get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        char* brand_ptr = brand;
        for (unsigned int i = 0; i < 3; i++) {
            unsigned int regs[4];
            __get_cpuid(0x80000002 + i, &regs[0], &regs[1], &regs[2], &regs[3]);
            memcpy(brand_ptr + i*16, regs, 16);
        }
        brand[48] = '\0';
    } else {
        char* strncpy(char* dest, const char* src, size_t n);
        strncpy(brand, vendor, sizeof(brand)-1);
        brand[sizeof(brand)-1] = '\0';
    }

    strncpyToUser(info->osname, "InstantOS", sizeof(info->osname));
    strncpyToUser(info->loggedOnUser, "user", sizeof(info->loggedOnUser));
    strncpyToUser(info->cpuname, brand, sizeof(info->cpuname));

    char buf[16];
    memset(buf, 0, sizeof(buf));
    uitoa(pmm.getTotalMemory() / (1024 * 1024), buf, sizeof(buf));    strncpyToUser(info->maxRamGB, buf, sizeof(info->maxRamGB));
    
    uint64_t usedBytes = pmm.getTotalMemory() - pmm.getFreeMemory();
    uint64_t usedMB = usedBytes / (1024 * 1024);    char buf2[16];
    memset(buf2, 0, sizeof(buf2));
    uitoa(usedMB, buf2, sizeof(buf2));
    strncpyToUser(info->usedRamGB, buf2, sizeof(info->usedRamGB));
    
    return 0;
}
