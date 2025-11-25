#pragma once

#include <cpu/idt/interrupt.hpp>
#include <graphics/console.hpp>
extern Console* console;

class Timer : public Interrupt {
public:
    void initialize() override {
        LAPIC& lapic = LAPIC::get();
        
        console->drawText("Timer: Starting calibration\n");
        
        lapic.setTimerDivide(0x03);
        
        lapic.write(0x320, 0x10000);
        lapic.write(0x380, 0xFFFFFFFF);
        
        console->drawText("Timer: Waiting for calibration...\n");
        
        for (volatile int i = 0; i < 10000000; i++) {
            asm volatile("pause");
        }

        uint32_t ticks = 0xFFFFFFFF - lapic.read(0x390);
        
        console->drawText("Ticks measured: ");
        console->drawHex(ticks);
        console->drawText("\n");

        if (ticks == 0) {
            tpus = 1000;
        } else {
            tpus = ticks / 100000;
            if (tpus == 0) {
                tpus = 1;
            }
        }

        console->drawText("TPUS: ");
        console->drawHex(tpus);
        console->drawText("\n");

        uint32_t count = tpus * 10000;

        console->drawText("Timer count: ");
        console->drawHex(count);
        console->drawText("\n");

        lapic.write(0x380, count);
        lapic.write(0x320, VECTOR_TIMER | 0x20000);

        console->drawText("Timer started\n");
    }
    
    void Run(InterruptFrame* frame) override {
        this->sendEOI();
    }

private:
    uint32_t tpus;
    int tick = 0;
};