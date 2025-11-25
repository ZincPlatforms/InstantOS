#pragma once

#include <cstdint>

class PCI {
public:
    static PCI& get();
    
    // Read from PCI configuration space
    uint8_t readConfig8(uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);
    uint16_t readConfig16(uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);
    uint32_t readConfig32(uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);
    
    // Write to PCI configuration space
    void writeConfig8(uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint8_t value);
    void writeConfig16(uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint16_t value);
    void writeConfig32(uint16_t segment, uint8_t bus, uint8_t device, uint8_t function, uint16_t offset, uint32_t value);
    
private:
    PCI() = default;
    
    // PCI configuration space I/O ports
    static constexpr uint16_t CONFIG_ADDRESS = 0xCF8;
    static constexpr uint16_t CONFIG_DATA = 0xCFC;
    
    // Build configuration address
    uint32_t makeAddress(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);
};
