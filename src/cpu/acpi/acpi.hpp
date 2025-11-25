#pragma once

#include <cstdint>
#include <cstddef>

class ACPI {
public:
    static ACPI& get();
    
    bool initialize();
    void shutdown();
    
    void enumerate();
    void reboot();
    void sysShutdown();
    
private:
    ACPI() = default;

    void* rsdp = nullptr;
    void* rsdt = nullptr;
    bool initialized = false;
};