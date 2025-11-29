#pragma once

#include <cpu/idt/interrupt.hpp>
#include <cpu/process/scheduler.hpp>
#include <cpu/apic/irqs.hpp>
#include <graphics/console.hpp>

extern Console* console;

class Timer : public Interrupt {
public:
    void initialize() override {
        LAPIC& lapic = LAPIC::get();
        
        lapic.setTimerDivide(0x03);
        
        lapic.write(0x320, 0x10000);
        lapic.write(0x380, 0xFFFFFFFF);
                
        for (int i = 0; i < 10000000; i++) {
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
        tick++;
        milliseconds++;
        
        if(tick % 10 == 0){
            Scheduler::get().schedule(frame);
        }
    }
    
    static Timer& get() {
        static Timer instance;
        return instance;
    }
    
    uint64_t getMilliseconds() const {
        return milliseconds;
    }
    
    uint64_t getTicks() const {
        return tick;
    }

private:
    uint32_t tpus;
    uint64_t tick = 0;
    uint64_t milliseconds = 0;
};