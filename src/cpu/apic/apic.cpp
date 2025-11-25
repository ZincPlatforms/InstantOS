#include "apic.hpp"
#include <cpu/mm/vmm.hpp>
#include <x86_64/requests.hpp>

extern "C" {
    #include <uacpi/tables.h>
    #include <uacpi/acpi.h>
}

extern VMM vmm;

LAPIC& LAPIC::get() {
    static LAPIC instance;
    return instance;
}

bool LAPIC::initialize() {
    uint32_t eax, edx;
    asm volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(0x1B));
    uint64_t apic_base_msr = (static_cast<uint64_t>(edx) << 32) | eax;
    
    apic_base_msr |= (1 << 11);
    
    eax = apic_base_msr & 0xFFFFFFFF;
    edx = apic_base_msr >> 32;
    asm volatile("wrmsr" :: "a"(eax), "d"(edx), "c"(0x1B));
    
    uacpi_table table;
    uacpi_status ret = uacpi_table_find_by_signature("APIC", &table);
    
    if (uacpi_unlikely_error(ret)) {
        return false;
    }
    
    struct madt_header {
        uint32_t lapic_address;
        uint32_t flags;
    } __attribute__((packed));
    
    auto* madt = reinterpret_cast<madt_header*>(
        reinterpret_cast<uint8_t*>(table.virt_addr) + 36
    );
    
    uint64_t lapic_phys = madt->lapic_address;
    uint64_t lapic_virt = lapic_phys + hhdm_request.response->offset;
    
    vmm.map(
        reinterpret_cast<void*>(lapic_virt),
        reinterpret_cast<void*>(lapic_phys),
        PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE
    );
    
    base = reinterpret_cast<volatile uint32_t*>(lapic_virt);
    initialized = true;
    
    uacpi_table_unref(&table);
    
    return true;
}

void LAPIC::enable() {
    if (!initialized) return;
    
    write(0x80, 0);
    
    uint32_t spurious = read(LAPIC_SPURIOUS);
    spurious |= 0x100;
    spurious |= 0xFF;
    write(LAPIC_SPURIOUS, spurious);
}

void LAPIC::sendEOI() {
    write(LAPIC_EOI, 0);
}

uint32_t LAPIC::getId() {
    if (!initialized) return 0;
    return read(LAPIC_ID) >> 24;
}

void LAPIC::setTimerDivide(uint8_t divide) {
    if (!initialized) return;
    write(LAPIC_TIMER_DIV, divide);
}

void LAPIC::startTimer(uint32_t initialCount, uint8_t vector, bool periodic) {
    if (!initialized) {
        return;
    }
    
    uint32_t mode = periodic ? 0x20000 : 0;
    uint32_t lvt = vector | mode;
    write(LAPIC_TIMER, lvt);
    write(LAPIC_TIMER_INITCNT, initialCount);
    

}

uint32_t LAPIC::read(uint32_t reg) {
    return base[reg / 4];
}

void LAPIC::write(uint32_t reg, uint32_t value) {
    base[reg / 4] = value;
    asm volatile("mfence" ::: "memory");
}

IOAPIC::IOAPIC(uint64_t physAddr, uint32_t gsiBase) : gsiBase(gsiBase) {
    uint64_t virt = physAddr + hhdm_request.response->offset;
    
    vmm.map(
        reinterpret_cast<void*>(virt),
        reinterpret_cast<void*>(physAddr),
        PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE
    );
    
    base = reinterpret_cast<volatile uint32_t*>(virt);
}

void IOAPIC::setRedirect(uint8_t irq, uint8_t vector, uint32_t lapicId, bool masked) {
    uint64_t entry = vector;
    
    entry |= (0ULL << 8);
    entry |= (0ULL << 11);
    entry |= (0ULL << 13);
    entry |= (0ULL << 15);
    
    if (masked) {
        entry |= (1ULL << 16);
    }
    
    entry |= (static_cast<uint64_t>(lapicId) << 56);
    
    uint32_t reg = IOAPIC_REG_REDTBL + (irq * 2);
    write(reg, static_cast<uint32_t>(entry));
    write(reg + 1, static_cast<uint32_t>(entry >> 32));
}

void IOAPIC::maskIRQ(uint8_t irq) {
    uint32_t reg = IOAPIC_REG_REDTBL + (irq * 2);
    uint32_t low = read(reg);
    write(reg, low | (1 << 16));
}

void IOAPIC::unmaskIRQ(uint8_t irq) {
    uint32_t reg = IOAPIC_REG_REDTBL + (irq * 2);
    uint32_t low = read(reg);
    write(reg, low & ~(1 << 16));
}

uint32_t IOAPIC::getMaxRedirect() {
    uint32_t ver = read(IOAPIC_REG_VER);
    return (ver >> 16) & 0xFF;
}

void IOAPIC::write(uint8_t reg, uint32_t value) {
    base[IOAPIC_REGSEL / 4] = reg;
    base[IOAPIC_IOWIN / 4] = value;
}

uint32_t IOAPIC::read(uint8_t reg) {
    base[IOAPIC_REGSEL / 4] = reg;
    return base[IOAPIC_IOWIN / 4];
}

APICManager& APICManager::get() {
    static APICManager instance;
    return instance;
}

bool APICManager::initialize() {
    if (initialized) return true;
    
    if (!LAPIC::get().initialize()) {
        return false;
    }
    
    LAPIC::get().enable();
    
    uacpi_table table;
    uacpi_status ret = uacpi_table_find_by_signature("APIC", &table);
    
    if (uacpi_unlikely_error(ret)) {
        return false;
    }
    
    uint8_t* madt_entries = reinterpret_cast<uint8_t*>(table.virt_addr) + 44;
    uint8_t* madt_end = reinterpret_cast<uint8_t*>(table.virt_addr) + table.hdr->length;
    
    while (madt_entries < madt_end) {
        uint8_t type = madt_entries[0];
        uint8_t length = madt_entries[1];
        
        if (type == 1 && ioapicCount < 16) {
            struct ioapic_entry {
                uint8_t type;
                uint8_t length;
                uint8_t id;
                uint8_t reserved;
                uint32_t address;
                uint32_t gsi_base;
            } __attribute__((packed));
            
            auto* entry = reinterpret_cast<ioapic_entry*>(madt_entries);
            ioapics[ioapicCount++] = new IOAPIC(entry->address, entry->gsi_base);
            
        } else if (type == 2 && overrideCount < 16) {
            struct iso_entry {
                uint8_t type;
                uint8_t length;
                uint8_t bus;
                uint8_t source;
                uint32_t gsi;
                uint16_t flags;
            } __attribute__((packed));
            
            auto* entry = reinterpret_cast<iso_entry*>(madt_entries);
            overrides[overrideCount].source = entry->source;
            overrides[overrideCount].gsi = entry->gsi;
            overrides[overrideCount].flags = entry->flags;
            overrideCount++;
        }
        
        madt_entries += length;
    }
    
    uacpi_table_unref(&table);
    
    initialized = true;
    return true;
}

uint32_t APICManager::resolveIRQ(uint8_t irq) {
    for (uint8_t i = 0; i < overrideCount; i++) {
        if (overrides[i].source == irq) {
            return overrides[i].gsi;
        }
    }
    return irq;
}

void APICManager::mapIRQ(uint8_t irq, uint8_t vector) {
    uint32_t gsi = resolveIRQ(irq);
    
    IOAPIC* ioapic = getIOAPICForGSI(gsi);
    if (!ioapic) {
        return;
    }
    
    uint32_t lapicId = LAPIC::get().getId();
    
    uint8_t redirectIndex = gsi - ioapic->getGSIBase();
    
    ioapic->setRedirect(redirectIndex, vector, lapicId, false);
}

IOAPIC* APICManager::getIOAPICForGSI(uint32_t gsi) {
    for (uint8_t i = 0; i < ioapicCount; i++) {
        uint32_t base = ioapics[i]->getGSIBase();
        uint32_t max = base + ioapics[i]->getMaxRedirect();
        
        if (gsi >= base && gsi <= max) {
            return ioapics[i];
        }
    }
    
    return nullptr;
}
