#include "requests.hpp"
#include <cpu/gdt/gdt.hpp>
#include <cpu/idt/idt.hpp>
#include <cpu/mm/memmgr.hpp>
#include <cpu/acpi/acpi.hpp>
#include <cpu/apic/apic.hpp>
#include <cpu/apic/irqs.hpp>
#include <cpu/pic.hpp>
#include <graphics/framebuffer.hpp>
#include <graphics/console.hpp>
#include <string.h>

#include <interrupts/timer.hpp>
#include <interrupts/keyboard.hpp>

Framebuffer* fb = nullptr;
Console* console = nullptr;
void _bsod();
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
    
    auto kb = new Keyboard();
    ISR::registerIRQ(VECTOR_KEYBOARD, kb);
    APICManager::get().mapIRQ(IRQ_KEYBOARD, VECTOR_KEYBOARD);
    
    asm volatile("sti");

    {
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

    _bsod();
//    console->drawText("YOOO");

    for(;;);

//     delete fb;
//     delete console;
}