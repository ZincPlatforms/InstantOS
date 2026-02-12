#include <instant/syscall.hpp>
#include <cstdio.hpp>

namespace {
    constexpr unsigned long HEAP_START = 0x0000600000000000UL;
    constexpr unsigned long HEAP_SIZE = 16 * 1024 * 1024;
    unsigned long heap_current = HEAP_START;
    bool heap_initialized = false;
    
    void init_heap() {
        if (!heap_initialized) {
            heap_initialized = true;
        }
    }
    
    void* allocate(unsigned long size) {
        init_heap();
        
        size = (size + 15) & ~15UL;
        
        if (heap_current + size >= HEAP_START + HEAP_SIZE) {
            return nullptr;
        }
        
        void* ptr = (void*)heap_current;
        heap_current += size;
        return ptr;
    }
}

void* operator new(unsigned long, void* ptr) noexcept {
    return ptr;
}

void* operator new[](unsigned long, void* ptr) noexcept {
    return ptr;
}

void* operator new(unsigned long size) {
    void* ptr = allocate(size);
    if (!ptr) {
        printf("Out of memory!\n");
        syscall0(0);
    }
    return ptr;
}

void* operator new[](unsigned long size) {
    return operator new(size);
}

void* operator new(unsigned long size, const void*) noexcept {
    return allocate(size);
}

void* operator new[](unsigned long size, const void*) noexcept {
    return allocate(size);
}

void operator delete(void*) noexcept {
}

void operator delete[](void*) noexcept {
}

void operator delete(void*, unsigned long) noexcept {
}

void operator delete[](void*, unsigned long) noexcept {
}
