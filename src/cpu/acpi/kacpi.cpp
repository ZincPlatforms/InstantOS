extern "C" {
    #include <uacpi/kernel_api.h>
    #include <uacpi/types.h>
}
#include <cpu/mm/pmm.hpp>
#include <cpu/mm/vmm.hpp>
#include <cpu/mm/heap.hpp>
#include <cpu/acpi/pci.hpp>
#include <x86_64/ports.hpp>
#include <cstdint>
#include <string.h>
#include <limine.h>

PCI& getPCI() {
    return PCI::get();
}

extern PMM pmm;
extern VMM vmm;
extern Heap kheap;
extern volatile limine_hhdm_request hhdm_request;

extern "C" {

void uacpi_kernel_log(uacpi_log_level level, const char* msg) {}

void* uacpi_kernel_alloc(uacpi_size size) {
    if (!kheap.isInitialized()) {
        return nullptr;
    }
    void* ptr = kheap.allocate(size);
    return ptr;
}

void uacpi_kernel_free(void* ptr) {
    if (!kheap.isInitialized() || !ptr) {
        return;
    }
    kheap.free(ptr);
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    if (!hhdm_request.response) {
        return nullptr;
    }
    
    uint64_t page_aligned_addr = addr & ~(PAGE_SIZE - 1);
    uint64_t offset = addr & (PAGE_SIZE - 1);
    size_t pages_needed = ((offset + len + PAGE_SIZE - 1) / PAGE_SIZE);
    
    for (size_t i = 0; i < pages_needed; i++) {
        uint64_t phys = page_aligned_addr + (i * PAGE_SIZE);
        void* phys_ptr = reinterpret_cast<void*>(phys);
        void* virt_ptr = reinterpret_cast<void*>(phys + hhdm_request.response->offset);
        
        vmm.map(virt_ptr, phys_ptr, PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE);
    }
    
    return reinterpret_cast<void*>(addr + hhdm_request.response->offset);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len) {}

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle* out_handle) {
    *out_handle = reinterpret_cast<uacpi_handle>(base);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {}

uacpi_status uacpi_kernel_io_read(uacpi_handle handle, uacpi_size offset, uacpi_u8 byte_width, uacpi_u64* value) {
    uint16_t port = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(handle) + offset);
    
    switch (byte_width) {
        case 1: {
            *value = inb(port);
            break;
        }
        case 2: {
            *value = inw(port);
            break;
        }
        case 4: {
            *value = inl(port);
            break;
        }
        default:
            return UACPI_STATUS_INVALID_ARGUMENT;
    }
    
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write(uacpi_handle handle, uacpi_size offset, uacpi_u8 byte_width, uacpi_u64 value) {
    uint16_t port = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(handle) + offset);
    
    switch (byte_width) {
        case 1:
            outb(port, static_cast<uint8_t>(value));
            break;
        case 2:
            outw(port, static_cast<uint16_t>(value));
            break;
        case 4:
            outl(port, static_cast<uint32_t>(value));
            break;
        default:
            return UACPI_STATUS_INVALID_ARGUMENT;
    }
    
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset, uacpi_u8* value) {
    uint16_t port = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(handle) + offset);
    *value = inb(port);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset, uacpi_u8 value) {
    uint16_t port = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(handle) + offset);
    outb(port, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset, uacpi_u16* value) {
    uint16_t port = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(handle) + offset);
    *value = inw(port);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset, uacpi_u16 value) {
    uint16_t port = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(handle) + offset);
    outw(port, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset, uacpi_u32* value) {
    uint16_t port = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(handle) + offset);
    *value = inl(port);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset, uacpi_u32 value) {
    uint16_t port = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(handle) + offset);
    outl(port, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle* out_handle) {
    *out_handle = reinterpret_cast<uacpi_handle>(
        (static_cast<uintptr_t>(address.segment) << 32) |
        (static_cast<uintptr_t>(address.bus) << 16) |
        (static_cast<uintptr_t>(address.device) << 8) |
        address.function
    );
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) {}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle device, uacpi_size offset, uacpi_u8* value) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(device);
    uint16_t segment = (addr >> 32) & 0xFFFF;
    uint8_t bus = (addr >> 16) & 0xFF;
    uint8_t dev = (addr >> 8) & 0xFF;
    uint8_t func = addr & 0xFF;
    
    extern class PCI& getPCI();
    *value = getPCI().readConfig8(segment, bus, dev, func, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle device, uacpi_size offset, uacpi_u8 value) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(device);
    uint16_t segment = (addr >> 32) & 0xFFFF;
    uint8_t bus = (addr >> 16) & 0xFF;
    uint8_t dev = (addr >> 8) & 0xFF;
    uint8_t func = addr & 0xFF;
    
    extern class PCI& getPCI();
    getPCI().writeConfig8(segment, bus, dev, func, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle device, uacpi_size offset, uacpi_u16* value) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(device);
    uint16_t segment = (addr >> 32) & 0xFFFF;
    uint8_t bus = (addr >> 16) & 0xFF;
    uint8_t dev = (addr >> 8) & 0xFF;
    uint8_t func = addr & 0xFF;
    
    extern class PCI& getPCI();
    *value = getPCI().readConfig16(segment, bus, dev, func, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle device, uacpi_size offset, uacpi_u16 value) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(device);
    uint16_t segment = (addr >> 32) & 0xFFFF;
    uint8_t bus = (addr >> 16) & 0xFF;
    uint8_t dev = (addr >> 8) & 0xFF;
    uint8_t func = addr & 0xFF;
    
    extern class PCI& getPCI();
    getPCI().writeConfig16(segment, bus, dev, func, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle device, uacpi_size offset, uacpi_u32* value) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(device);
    uint16_t segment = (addr >> 32) & 0xFFFF;
    uint8_t bus = (addr >> 16) & 0xFF;
    uint8_t dev = (addr >> 8) & 0xFF;
    uint8_t func = addr & 0xFF;
    
    extern class PCI& getPCI();
    *value = getPCI().readConfig32(segment, bus, dev, func, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle device, uacpi_size offset, uacpi_u32 value) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(device);
    uint16_t segment = (addr >> 32) & 0xFFFF;
    uint8_t bus = (addr >> 16) & 0xFF;
    uint8_t dev = (addr >> 8) & 0xFF;
    uint8_t func = addr & 0xFF;
    
    extern class PCI& getPCI();
    getPCI().writeConfig32(segment, bus, dev, func, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out_rsdp_address) {
    extern volatile limine_rsdp_request rsdp_request;
    
    if (rsdp_request.response == nullptr || rsdp_request.response->address == nullptr) {
        return UACPI_STATUS_NOT_FOUND;
    }
    
    void* rsdp_virt = rsdp_request.response->address;
    uint64_t rsdp_phys = reinterpret_cast<uint64_t>(rsdp_virt) - hhdm_request.response->offset;
    
    *out_rsdp_address = rsdp_phys;
    return UACPI_STATUS_OK;
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    return 0;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    for (uint8_t i = 0; i < usec; i++) {
        for (int j = 0; j < 1000; j++) {
            asm volatile("pause");
        }
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    for (uacpi_u64 i = 0; i < msec; i++) {
        uacpi_kernel_stall(250);
        uacpi_kernel_stall(250);
        uacpi_kernel_stall(250);
        uacpi_kernel_stall(250);
    }
}

struct mutex_stub {
    bool locked;
};

struct spinlock_stub {
    bool locked;
};

struct event_stub {
    bool signaled;
};

uacpi_handle uacpi_kernel_create_mutex(void) {
    auto* mtx = static_cast<mutex_stub*>(kheap.allocate(sizeof(mutex_stub)));
    if (mtx) {
        mtx->locked = false;
    }
    return mtx;
}

void uacpi_kernel_free_mutex(uacpi_handle handle) {
    kheap.free(handle);
}

uacpi_handle uacpi_kernel_create_event(void) {
    auto* evt = static_cast<event_stub*>(kheap.allocate(sizeof(event_stub)));
    if (evt) {
        evt->signaled = false;
    }
    return evt;
}

void uacpi_kernel_free_event(uacpi_handle handle) {
    kheap.free(handle);
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    auto* lock = static_cast<spinlock_stub*>(kheap.allocate(sizeof(spinlock_stub)));
    if (lock) {
        lock->locked = false;
    }
    return lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
    kheap.free(handle);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    auto* lock = static_cast<spinlock_stub*>(handle);
    lock->locked = true;
    return 0;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    auto* lock = static_cast<spinlock_stub*>(handle);
    lock->locked = false;
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout) {
    auto* mtx = static_cast<mutex_stub*>(handle);
    mtx->locked = true;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
    auto* mtx = static_cast<mutex_stub*>(handle);
    mtx->locked = false;
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout) {
    auto* evt = static_cast<event_stub*>(handle);
    (void)timeout;
    return evt->signaled ? UACPI_TRUE : UACPI_FALSE;
}

void uacpi_kernel_signal_event(uacpi_handle handle) {
    auto* evt = static_cast<event_stub*>(handle);
    evt->signaled = true;
}

void uacpi_kernel_reset_event(uacpi_handle handle) {
    auto* evt = static_cast<event_stub*>(handle);
    evt->signaled = false;
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    static int num = 1; 
    return (void*)&num;
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type, uacpi_work_handler handler, uacpi_handle ctx) {
    handler(ctx);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx, uacpi_handle* out_irq_handle) {
    *out_irq_handle = reinterpret_cast<uacpi_handle>(static_cast<uintptr_t>(irq));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle irq_handle) {
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* req) {
    switch (req->type) {
        case UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT:
            asm volatile("int3");
            return UACPI_STATUS_OK;
            
        case UACPI_FIRMWARE_REQUEST_TYPE_FATAL:
            asm volatile("cli; hlt");
            return UACPI_STATUS_OK;
            
        default:
            return UACPI_STATUS_UNIMPLEMENTED;
    }
}

}