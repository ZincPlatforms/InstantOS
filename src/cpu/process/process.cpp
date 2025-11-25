#include "process.hpp"
#include "../mm/pmm.hpp"
#include <x86_64/requests.hpp>


Process::Process(uint32_t pid) : pid(pid), state(ProcessState::Ready), next(nullptr), kernelStack(0), userStack(0), fpuState(nullptr) {
    vmm.init();
    vmm.cloneKernelMappings();
    
    void* kstackPhys = pmm.allocatePages(4);
    if (kstackPhys) {
        uint64_t kstackVirt = reinterpret_cast<uint64_t>(kstackPhys) + hhdm_request.response->offset;
        kernelStack = kstackVirt + (4 * PAGE_SIZE);
    }
    
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
    
    context.fpuStatePtr = reinterpret_cast<uint64_t>(fpuState);
    
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
    
    if (fpuState) {
        void* fpuPhys = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(fpuState) - hhdm_request.response->offset);
        pmm.freePage(fpuPhys);
    }
}
