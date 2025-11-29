#include "requests.hpp"
#include <cpu/gdt/gdt.hpp>
#include <cpu/idt/idt.hpp>
#include <cpu/syscall/syscall.hpp>
#include <cpu/mm/memmgr.hpp>
#include <cpu/acpi/acpi.hpp>
#include <cpu/apic/apic.hpp>
#include <cpu/apic/irqs.hpp>
#include <cpu/pic.hpp>
#include <cpu/process/scheduler.hpp>
#include <cpu/process/exec.hpp>
#include <graphics/framebuffer.hpp>
#include <graphics/console.hpp>
#include <string.h>

#include <interrupts/timer.hpp>
#include <interrupts/keyboard.hpp>

#include <fs/vfs/vfs.hpp>
#include <fs/initrd/initrd.hpp>
#include <fs/ramfs/ramfs.hpp>

Framebuffer* fb = nullptr;
Console* console = nullptr;
GDT* gdt = nullptr;
Keyboard* globalKeyboard = nullptr;
Timer* globalTimer = nullptr;

int main();

extern "C" void enterUsermode(uint64_t entry, uint64_t stack);
extern "C" void _kinit(){
    asm volatile("cli");
    
    static GDT _gdt;
    gdt = &_gdt;
    static IDT _idt;
    
    MemoryManager mm;

    fb = new Framebuffer();
    console = new Console(fb);
        
    if (ACPI::get().initialize()) {
        console->drawText("ACPI initialized successfully.\n");
    } else {
        console->drawText("ACPI failed to initialize.\n");
    }

    PIC::disable();

    if (APICManager::get().initialize()) {
        console->drawText("APIC initialized successfully.\n");
    } else {
        console->drawText("APIC initialization failed.\n");
    }
    
    Scheduler::get().initialize();
    
    globalTimer = new Timer();
    ISR::registerIRQ(VECTOR_TIMER, globalTimer);
    globalTimer->initialize();
    
    globalKeyboard = new Keyboard();
    ISR::registerIRQ(VECTOR_KEYBOARD, globalKeyboard);
    APICManager::get().mapIRQ(IRQ_KEYBOARD, VECTOR_KEYBOARD);
    Syscall::get().initialize();
    
    VFS::get().initialize();
    
    if (module_request.response && module_request.response->module_count > 0) {
        auto* module = module_request.response->modules[0];
        
        InitrdFS* initrd = new InitrdFS(module->address, module->size);
        if (VFS::get().mount(initrd, "/") == 0) {
            console->drawText("Initrd mounted at /\n");
        } else {
            console->drawText("Failed to mount initrd\n");
        }
    }
    
    RamFS* ramfs = new RamFS();
    if (VFS::get().mount(ramfs, "/tmp") == 0) {
        console->drawText("RamFS mounted at /tmp\n");
    }
    
    int returnCode = main();

    for(;;);

}