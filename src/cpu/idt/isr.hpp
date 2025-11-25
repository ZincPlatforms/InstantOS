#pragma once

#include <cstdint>
#include "interrupt.hpp"

extern "C" void exceptionHandler(InterruptFrame* frame);
extern "C" void irqHandler(InterruptFrame* frame);

class ISR {
public:
    static void registerIRQ(uint8_t vector, Interrupt* handler);
};