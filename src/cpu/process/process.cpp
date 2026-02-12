#include "process.hpp"
#include <cpu/mm/pmm.hpp>
#include <x86_64/requests.hpp>
#include <cpu/gdt/gdt.hpp>
#include <cpu/syscall/syscall.hpp>
#include <graphics/console.hpp>

extern Console* console;
extern "C" void enterUsermode(uint64_t entry, uint64_t stack);

constexpr uint64_t USER_STACK_TOP = 0x00007FFFFFFFE000;
constexpr size_t USER_STACK_PAGES = 4;

Process::Process(uint32_t pid) : pid(pid), parentPID(0), next(nullptr), exitCode(0), state(ProcessState::Ready), kernelStack(0), userStack(0), fpuState(nullptr), validUserState(false) {
    for (int i = 0; i < NSIG; i++) {
        signalHandler.handlers[i] = nullptr;
    }
    signalHandler.pending = 0;
    signalHandler.blocked = 0;
    vmm.init();
    vmm.cloneKernelMappings();
    
    void* kstackPhys = pmm.allocatePages(4);
    if (kstackPhys) {
        uint64_t kstackVirt = reinterpret_cast<uint64_t>(kstackPhys) + hhdm_request.response->offset;
        kernelStack = kstackVirt + (4 * PAGE_SIZE);
        if (console) {
            console->drawText("[PROCESS] PID=");
            console->drawNumber(pid);
            console->drawText(" KernelStack=");
            console->drawHex(kernelStack);
            console->drawText("\n");
        }
    }
    
    void* ustackPhys = pmm.allocatePages(USER_STACK_PAGES);
    if (ustackPhys) {
        uint64_t ustackBase = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
        vmm.mapRange(reinterpret_cast<void*>(ustackBase), ustackPhys, USER_STACK_PAGES, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        userStack = USER_STACK_TOP - 8;    }
    
    void* fpuPhys = pmm.allocatePage();
    if (fpuPhys) {
        fpuState = reinterpret_cast<FPUState*>(reinterpret_cast<uint64_t>(fpuPhys) + hhdm_request.response->offset);
    }
    
    context.rax = 0;
    context.rbx = 0;
    context.rcx = 0;
    context.rdx = 0;
    context.rsi = 0;
    context.rdi = 0;
    context.rbp = 0;
    context.rsp = kernelStack;
    context.r8 = 0;
    context.r9 = 0;
    context.r10 = 0;
    context.r11 = 0;
    context.r12 = 0;
    context.r13 = 0;
    context.r14 = 0;
    context.r15 = 0;
    context.rip = 0;
    context.rflags = 0x202;
    
    uint64_t pml4Virt = reinterpret_cast<uint64_t>(vmm.getPageTable());
    uint64_t pml4Phys = pml4Virt - hhdm_request.response->offset;
    context.cr3 = pml4Phys;
    
    context.fxstate = reinterpret_cast<uint64_t>(fpuState);
    
    if (fpuState) {
        asm volatile("fxsave (%0)" : : "r"(fpuState));
    }
}

Process::~Process() {
    if (kernelStack) {
        uint64_t kstackVirt = kernelStack - (4 * PAGE_SIZE);
        void* kstackPhys = reinterpret_cast<void*>(kstackVirt - hhdm_request.response->offset);
        pmm.freePages(kstackPhys, 4);
    }
    
    if (userStack) {
        uint64_t ustackBase = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE);
        void* ustackVirt = reinterpret_cast<void*>(ustackBase);
        void* ustackPhys = vmm.getPhysical(ustackVirt);
        if (ustackPhys) {
            vmm.unmapRange(ustackVirt, USER_STACK_PAGES);
            pmm.freePages(ustackPhys, USER_STACK_PAGES);
        }
    }
    
    if (fpuState) {
        void* fpuPhys = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(fpuState) - hhdm_request.response->offset);
        pmm.freePage(fpuPhys);
    }
}

void Process::jumpToUsermode(uint64_t entry, GDT* gdt) {    
    vmm.load();
    
    if (gdt) {
        gdt->setKernelStack(kernelStack);
    }
    
    Syscall::get().setKernelStack(kernelStack);
    
    context.rip = entry;
    context.rsp = userStack;
    
    enterUsermode(entry, userStack);
}

void Process::sendSignal(int sig) {
    if (sig < 0 || sig >= NSIG) return;
    signalHandler.pending |= (1ULL << sig);
}

void Process::handlePendingSignals() {
    if (!signalHandler.pending) return;
    
    for (int sig = 0; sig < NSIG; sig++) {
        if (!(signalHandler.pending & (1ULL << sig))) continue;
        if (signalHandler.blocked & (1ULL << sig)) continue;
        
        signalHandler.pending &= ~(1ULL << sig);
        
        if (sig == SIGKILL) {
            state = ProcessState::Terminated;
            exitCode = 128 + sig;
            return;
        }
        
        sighandler_t handler = signalHandler.handlers[sig];
        if (!handler) {
            state = ProcessState::Terminated;
            exitCode = 128 + sig;
            return;
        }
        
        uint64_t oldRsp = context.rsp;
        context.rsp -= 128;
        context.rsp &= ~0xFULL;
        
        uint64_t* stack = reinterpret_cast<uint64_t*>(context.rsp);
        stack[0] = context.rip;
        
        context.rip = reinterpret_cast<uint64_t>(handler);
        context.rdi = sig;
        
        break;
    }
}
