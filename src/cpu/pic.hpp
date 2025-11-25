#pragma once

#include <cstdint>
#include <x86_64/ports.hpp>

class PIC {
public:
    static void disable() {
        outb(0xA1, 0xFF);
        outb(0x21, 0xFF);
    }
};
