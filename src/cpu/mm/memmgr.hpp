#pragma once

#include <cstdint>
#include <cstddef>

class MemoryManager {
private:
    uint64_t totalMemory = 0;
    uint64_t highestAddress = 0;
    static constexpr uint64_t KERNEL_HEAP_START = 0xFFFF900000000000;
    static constexpr size_t INITIAL_HEAP_SIZE = 1 * 1024 * 1024;
public:
    MemoryManager();
    uint64_t getTotalMemory() const;
};