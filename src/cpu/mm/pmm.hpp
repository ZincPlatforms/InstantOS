#pragma once

#include "bitmap.hpp"
#include <cstdint>
#include <cstddef>

constexpr size_t PAGE_SIZE = 4096;

class PMM {
public:
    PMM() : intialized(false), availableMemory(0), usedMemory(0), 
            freeMemory(0), pages(0) {}

    void init(uint8_t* bmpBuffer, uint64_t maxMemory);

    void* allocatePage();
    void* allocatePages(size_t count);
    void freePage(void* page);
    void freePages(void* page, size_t count);
    void reservePage(void* page);
    void reservePages(void* page, size_t count);
    void reserveRegion(uint64_t base, uint64_t length);
    
    uint64_t getTotalMemory() const { return availableMemory; }
    uint64_t getUsedMemory() const { return usedMemory; }
    uint64_t getFreeMemory() const { return freeMemory; }
    uint64_t getReservedMemory() const { return reservedMemory; }
    size_t getPageCount() const { return pages; }
    
    bool isInitialized() const { return intialized; }
    
private:
    Bitmap bitmap;
    bool intialized;

    uint64_t availableMemory;
    uint64_t reservedMemory;
    uint64_t usedMemory;
    uint64_t freeMemory;
    size_t pages;

    size_t addressToIndex(void* addr) const {
        return reinterpret_cast<uint64_t>(addr) / PAGE_SIZE;
    }

    void* indexToAddress(size_t index) const {
        return reinterpret_cast<void*>(index * PAGE_SIZE);
    }
};

extern PMM pmm;