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

Framebuffer* fb = nullptr;
Console* console = nullptr;
void _bsod();

void task1() {
    int counter = 0;
    while(1) {
        counter++;
        if(counter % 10000000 != 0) continue;
        console->drawText("Task 1: ");
        console->drawNumber(counter);
        console->drawChar('\n');
    }
}

void task2() {
    int counter = 0;
    while(1) {
        counter++;
        if(counter % 10000000 != 0) continue;
        console->drawText("Task 2: ");
        console->drawNumber(counter);
        console->drawChar('\n');
    }
}


extern "C" void _kinit(){
    asm volatile("cli");
    
    GDT gdt;
    IDT idt;
    
    MemoryManager mm;

    fb = new Framebuffer();
    console = new Console(fb);
    console->setTextColor(0xFF0000);
    fb->clear(0x00FF00);
        
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
    ISR::registerIRQ(VECTOR_TIMER, new Timer());
    auto kb = new Keyboard();
    ISR::registerIRQ(VECTOR_KEYBOARD, kb);
    APICManager::get().mapIRQ(IRQ_KEYBOARD, VECTOR_KEYBOARD);
    Syscall::get().initialize();
    
    asm volatile("sti");

    Process* p1 = ProcessExecutor::createKernelProcess(task1);
    Process* p2 = ProcessExecutor::createKernelProcess(task2);
    
    Scheduler::get().addProcess(p1);
    Scheduler::get().addProcess(p2);
    
    Scheduler::get().schedule();

    if(false){
        fb->clear(0x000000);
        console->setTextColor(0xffffff);
        console->drawText("Welcome to InstantOS 0.0.1\n> ");
        char textbuf[1024];
        int size = 0;
        while(true){
            if (kb->hasKey()) {
                char k = kb->getKey();

                switch (k) {
                case '\n': {
                    console->drawText("\n");
                    textbuf[size++] = '\0';

                    if(textbuf[0] == 'b' && textbuf[1] == 's' && textbuf[2] == 'o' && textbuf[3] == 'd') {
                        _bsod();
                        while(1);
                    } else if(textbuf[0] == 'e' && textbuf[1] == 'c' && textbuf[2] == 'h' && textbuf[3] == 'o') {
                        int i = 5;

                        while(true){
                            if(textbuf[i] == '\0') break;
                            const char hi[2] = {textbuf[i++], '\0'};
                            console->drawText(hi);
                        }

                    } else if(textbuf[0] == 'h' && textbuf[1] == 'e' && textbuf[2] == 'l' && textbuf[3] == 'p') {
                        console->drawText("InstantOS Shell 0.1\n");
                        console->drawText("\t- bsod: trigger a bsod (windows edition)\n");
                        console->drawText("\t- echo: print text");
                    }
                    console->drawText("\n> ");

                    size = 0;
                } break;
                
                default:
                    char temp[2] = {k, '\0'};
                    console->drawText(temp);
                    if(k != '\b')
                        textbuf[size++] = k;
                    else {
                        --size;
                    }
                    break;
                }
            }
            asm volatile("hlt");
        }
    }
//    console->drawText("YOOO");

    for(;;);

//     delete fb;
//     delete console;
}