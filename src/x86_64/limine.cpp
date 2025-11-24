#include "requests.hpp"
#include <cpu/gdt/gdt.hpp>
#include <cpu/idt/idt.hpp>
#include <graphics/framebuffer.hpp>
#include <graphics/console.hpp>

extern "C" void _kinit(){
    GDT gdt;
    IDT idt;

    Framebuffer fb;
    Console console(&fb);
    console.setTextColor({ 255, 0, 0 });

    fb.clear(Color{0, 255, 0});

    console.drawText("YOOO");
}