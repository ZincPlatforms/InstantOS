#include "exec.hpp"
#include "../mm/pmm.hpp"
#include "../gdt/gdt.hpp"
#include "../syscall/syscall.hpp"
#include <x86_64/requests.hpp>
#include <string.h>
#include <fs/vfs/vfs.hpp>
#include <fs/elf/elf.hpp>

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

Process* ProcessExecutor::createUserProcess(uint64_t entry) {
    uint32_t pid = Scheduler::get().allocatePID();
    Process* proc = new Process(pid);
    
    uint64_t stack = proc->getUserStack();
    stack &= ~0xFULL;
    
    proc->getContext()->rip = entry;
    proc->getContext()->rsp = stack;
    proc->getContext()->rbp = 0;
    proc->getContext()->rflags = 0x202;
    
    return proc;
}

void ProcessExecutor::executeUserProcess(Process* proc, GDT* gdt) {
    proc->jumpToUsermode(proc->getContext()->rip, gdt);
}

constexpr uint64_t USER_CODE_BASE = 0x400000;

extern "C" void processTrampoline();

Process* ProcessExecutor::createUserProcessWithCode(void* code, size_t codeSize) {
    uint32_t pid = Scheduler::get().allocatePID();
    
    Process* proc = new Process(pid);
    size_t pages = (codeSize + PAGE_SIZE - 1) / PAGE_SIZE;
    void* codePhys = pmm.allocatePages(pages);
    
    if (codePhys) {
        uint64_t codeVirt = reinterpret_cast<uint64_t>(codePhys) + hhdm_request.response->offset;
        
        memset(reinterpret_cast<void*>(codeVirt), 0, pages * PAGE_SIZE);
        memcpy(reinterpret_cast<void*>(codeVirt), code, codeSize);
        
        proc->getVMM()->mapRange(reinterpret_cast<void*>(USER_CODE_BASE), codePhys, pages, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }
    
    uint64_t userStack = proc->getUserStack();
    userStack &= ~0xFULL;

    uint64_t entry = USER_CODE_BASE;
    uint64_t trampolineAddr = reinterpret_cast<uint64_t>(&processTrampoline);
    
    uint64_t kernelStack = proc->getKernelStack();
    kernelStack -= 8;
    *reinterpret_cast<uint64_t*>(kernelStack) = userStack;
    kernelStack -= 8;
    *reinterpret_cast<uint64_t*>(kernelStack) = entry;
    
    proc->getContext()->rip = trampolineAddr;
    proc->getContext()->rsp = kernelStack;
    proc->getContext()->rbp = 0;
    proc->getContext()->rflags = 0x202;
    
    return proc;
}

Process* ProcessExecutor::loadUserBinary(const char* path) {
    FileDescriptor* fd = nullptr;
    int result = VFS::get().open(path, 0, &fd);
    
    if (result != 0 || !fd) {
        return nullptr;
    }
    
    FileStats stats;
    if (fd->getNode()->ops->stat(fd->getNode(), &stats) != 0) {
        VFS::get().close(fd);
        return nullptr;
    }
    
    size_t size = stats.size;
    
    void* buffer = new uint8_t[size];
    if (VFS::get().read(fd, buffer, size) != static_cast<int64_t>(size)) {
        delete[] static_cast<uint8_t*>(buffer);
        VFS::get().close(fd);
        return nullptr;
    }
    
    VFS::get().close(fd);
    
    Process* proc = nullptr;
    if (ELFLoader::isValidELF(buffer, size)) {
        proc = ELFLoader::loadELF(buffer, size);
    } else {
        proc = createUserProcessWithCode(buffer, size);
    }
    
    delete[] static_cast<uint8_t*>(buffer);
    
    return proc;
}

void ProcessExecutor::setupArguments(Process* proc, int argc, const char** argv) {
    if (!proc || argc < 0) return;
    
    uint64_t userStack = proc->getUserStack();
    userStack &= ~0xFULL;
    
    size_t totalStringSize = 0;
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            totalStringSize += strlen(argv[i]) + 1;
        }
    }
    totalStringSize = (totalStringSize + 7) & ~7;
    
    size_t totalSize = totalStringSize + (argc + 1) * sizeof(uint64_t) + sizeof(uint64_t);
    
    userStack -= totalSize;
    userStack &= ~0xFULL;
    
    uint8_t* buffer = new uint8_t[totalSize];
    if (!buffer) return;
    memset(buffer, 0, totalSize);
    
    uint64_t stringBase = userStack + sizeof(uint64_t) + (argc + 1) * sizeof(uint64_t);
    size_t stringOffset = 0;
    
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            size_t len = strlen(argv[i]) + 1;
            memcpy(buffer + sizeof(uint64_t) + (argc + 1) * sizeof(uint64_t) + stringOffset, argv[i], len);
            
            uint64_t* argvPtr = reinterpret_cast<uint64_t*>(buffer + sizeof(uint64_t) + i * sizeof(uint64_t));
            *argvPtr = stringBase + stringOffset;
            stringOffset += len;
        }
    }
    
    uint64_t* nullPtr = reinterpret_cast<uint64_t*>(buffer + sizeof(uint64_t) + argc * sizeof(uint64_t));
    *nullPtr = 0;
    
    uint64_t* argcPtr = reinterpret_cast<uint64_t*>(buffer);
    *argcPtr = argc;
    
    uint64_t savedCR3;
    asm volatile("mov %%cr3, %0" : "=r"(savedCR3));
    
    proc->getVMM()->load();
    
    memcpy(reinterpret_cast<void*>(userStack), buffer, totalSize);
    
    asm volatile("mov %0, %%cr3" :: "r"(savedCR3) : "memory");
    
    delete[] buffer;
    
    proc->setUserStack(userStack);
   
    uint64_t* userRspOnStack = reinterpret_cast<uint64_t*>(proc->getContext()->rsp + 8);
    uint64_t oldValue = *userRspOnStack;
    *userRspOnStack = userStack;
}

Process* ProcessExecutor::createUserProcessWithArgs(void* code, size_t codeSize, int argc, const char** argv) {
    Process* proc = createUserProcessWithCode(code, codeSize);
    
    if (proc) {
        setupArguments(proc, argc, argv);
    }
    return proc;
}

Process* ProcessExecutor::loadUserBinaryWithArgs(const char* path, int argc, const char** argv) {
    FileDescriptor* fd = nullptr;
    int result = VFS::get().open(path, 0, &fd);
    
    if (result != 0 || !fd) {
        return nullptr;
    }
    
    FileStats stats;
    if (fd->getNode()->ops->stat(fd->getNode(), &stats) != 0) {
        VFS::get().close(fd);
        return nullptr;
    }
    
    size_t size = stats.size;
    
    void* buffer = new uint8_t[size];
    if (VFS::get().read(fd, buffer, size) != static_cast<int64_t>(size)) {
        delete[] static_cast<uint8_t*>(buffer);
        VFS::get().close(fd);
        return nullptr;
    }
    
    VFS::get().close(fd);
    
    Process* proc = nullptr;
    if (ELFLoader::isValidELF(buffer, size)) {
        proc = ELFLoader::loadELFWithArgs(buffer, size, argc, argv);
    } else {
        proc = createUserProcessWithArgs(buffer, size, argc, argv);
    }
    
    delete[] static_cast<uint8_t*>(buffer);
    
    return proc;
}
