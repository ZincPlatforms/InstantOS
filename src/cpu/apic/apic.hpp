#pragma once

#include <cstdint>
#include "lapic.hpp"
#include "ioapic.hpp"

class APICManager {
public:
    static APICManager& get();
    
    bool initialize();
    void mapIRQ(uint8_t irq, uint8_t vector);
    
private:
    APICManager() : initialized(false), ioapicCount(0), overrideCount(0) {}
    
    struct InterruptOverride {
        uint8_t source;
        uint32_t gsi;
        uint16_t flags;
    };
    
    bool initialized;
    IOAPIC* ioapics[16];
    uint8_t ioapicCount;
    InterruptOverride overrides[16];
    uint8_t overrideCount;
    
    IOAPIC* getIOAPICForGSI(uint32_t gsi);
    uint32_t resolveIRQ(uint8_t irq);
};
