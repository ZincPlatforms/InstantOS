#pragma once

#include <cpu/idt/interrupt.hpp>
#include <cpu/process/scheduler.hpp>
#include <graphics/console.hpp>
extern Console* console;

class Timer : public Interrupt {
public:
    void initialize() override {
        LAPIC& lapic = LAPIC::get();
        
        lapic.setTimerDivide(0x03);
        
        lapic.write(0x320, 0x10000);
        lapic.write(0x380, 0xFFFFFFFF);
                
        for (volatile int i = 0; i < 10000000; i++) {
            asm volatile("pause");
        }

        uint32_t ticks = lapic.read(0x390);
        uint32_t elapsed = 0xFFFFFFFF - ticks;

        if (elapsed == 0) {
            tpus = 1000;
        } else {
            tpus = elapsed / 100000;
            if (tpus == 0) {
                tpus = 1;
            }
        }

        uint32_t count = tpus * 10000;

        lapic.write(0x380, count);
        lapic.write(0x320, VECTOR_TIMER | 0x20000);
    }
    
    void Run(InterruptFrame* frame) override {
        this->sendEOI();
        if(tick++ % 50 == 0){
            console->drawText("Switching...\n");
            Scheduler::get().schedule();
        }
    }

private:
    uint32_t tpus;
    int tick = 0;
};