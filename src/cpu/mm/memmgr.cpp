#include "pmm.hpp"
#include "vmm.hpp"
#include "heap.hpp"
#include "memmgr.hpp"
#include <x86_64/requests.hpp>

static uint8_t PMMBMP[1024 * 1024] __attribute__((aligned(4096)));

MemoryManager::MemoryManager(){
    limine_memmap_response* memmap = memorymap_request.response;
    if (!memmap) {
        return;
    }
    
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        
        if (entry->base + entry->length > highestAddress) {
            highestAddress = entry->base + entry->length;
        }
        
        if (entry->type == LIMINE_MEMMAP_USABLE ||
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            totalMemory += entry->length;
        }
    }

    pmm.init(PMMBMP, highestAddress);

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_addr = entry->base;
            uint64_t end_addr = entry->base + entry->length;
            
            for (uint64_t addr = start_addr; addr < end_addr; addr += PAGE_SIZE) {
                pmm.freePage(reinterpret_cast<void*>(addr));
            }
        }
    }
    
    PageTable* pageTable = VMM::getCurrentPageTable();
    vmm.init(pageTable);
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        
        if (entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
            entry->type == LIMINE_MEMMAP_FRAMEBUFFER) {
                
            uint64_t base = entry->base & ~(PAGE_SIZE - 1);
            uint64_t length = ((entry->base + entry->length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)) - base;
            size_t page_count = length / PAGE_SIZE;

            pmm.reservePages(reinterpret_cast<void*>(base), page_count);

            for (size_t j = 0; j < page_count; j++) {
                void* addr = reinterpret_cast<void*>(base + j * PAGE_SIZE);
                vmm.map(addr, addr, PTE_PRESENT | PTE_WRITABLE);
            }
        }
    }

    size_t pages = (INITIAL_HEAP_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    
    void* phys = pmm.allocatePages(pages);
    if (!phys) {
        return;
    }
    
    void* virt = reinterpret_cast<void*>(KERNEL_HEAP_START);
    if (!vmm.mapRange(virt, phys, pages, PTE_PRESENT | PTE_WRITABLE)) {
        pmm.freePages(phys, pages);
        return;
    }
    
    kheap.init(virt, INITIAL_HEAP_SIZE);
}