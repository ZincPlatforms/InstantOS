#pragma once

// ACPI table layouts and the pure (hardware-independent) validation and
// FADT register-selection logic shared by the kernel ACPI driver
// (src/cpu/acpi/acpi.cpp) and the host-side fixture tests
// (tools/acpi-fixtures/acpi_fixture_tests.cpp).
//
// Nothing in this header performs port I/O, MMIO, or memory mapping, so it can
// be compiled and exercised on the host against synthetic ACPI 1.0/2.0/6.0
// firmware images. The struct layouts are locked with static_assert so the
// kernel and the tests can never silently disagree about field offsets.

#include <stdint.h>
#include <stddef.h>

// The kernel provides these in <common/string.hpp>; the host test provides
// thin wrappers over <cstring>. Either way strncmp/memcmp must be available
// before this header is included.

namespace acpi {

struct Rsdp {
    char signature[8];
    uint8_t checksum;
    char oemId[6];
    uint8_t revision;
    uint32_t rsdtAddress;
} __attribute__((packed));

struct Rsdp20 {
    Rsdp firstPart;
    uint32_t length;
    uint64_t xsdtAddress;
    uint8_t extendedChecksum;
    uint8_t reserved[3];
} __attribute__((packed));

struct AcpiHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemId[6];
    char oemTableId[8];
    uint32_t oemRevision;
    uint32_t creatorId;
    uint32_t creatorRevision;
} __attribute__((packed));

struct Rsdt {
    AcpiHeader header;
    uint32_t pointers[];
} __attribute__((packed));

struct Xsdt {
    AcpiHeader header;
    uint64_t pointers[];
} __attribute__((packed));

struct GenericAddressStructure {
    uint8_t addressSpace;
    uint8_t bitWidth;
    uint8_t bitOffset;
    uint8_t accessSize;
    uint64_t address;
} __attribute__((packed));

// MCFG: PCI Express Memory Mapped Configuration Space Base Address Description
// Table (ACPI 6.x, "Static Resource Allocation Structures"). The fixed header
// is followed by one allocation entry per (segment, bus-range) ECAM window.
struct McfgAllocation {
    uint64_t baseAddress;
    uint16_t segmentGroup;
    uint8_t startBus;
    uint8_t endBus;
    uint32_t reserved;
} __attribute__((packed));

struct Mcfg {
    AcpiHeader header;
    uint64_t reserved;
    McfgAllocation allocations[];
} __attribute__((packed));

static_assert(sizeof(McfgAllocation) == 16);
static_assert(offsetof(Mcfg, allocations) == 44);

struct Fadt {
    AcpiHeader header;
    uint32_t firmwareCtrl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t preferredPowerManagementProfile;
    uint16_t sciInterrupt;
    uint32_t smiCommandPort;
    uint8_t acpiEnable;
    uint8_t acpiDisable;
    uint8_t s4BiosReq;
    uint8_t pstateControl;
    uint32_t pm1aEventBlock;
    uint32_t pm1bEventBlock;
    uint32_t pm1aControlBlock;
    uint32_t pm1bControlBlock;
    uint32_t pm2ControlBlock;
    uint32_t pmTimerBlock;
    uint32_t gpe0Block;
    uint32_t gpe1Block;
    uint8_t pm1EventLength;
    uint8_t pm1ControlLength;
    uint8_t pm2ControlLength;
    uint8_t pmTimerLength;
    uint8_t gpe0Length;
    uint8_t gpe1Length;
    uint8_t gpe1Base;
    uint8_t cStateControl;
    uint16_t worstC2Latency;
    uint16_t worstC3Latency;
    uint16_t flushSize;
    uint16_t flushStride;
    uint8_t dutyOffset;
    uint8_t dutyWidth;
    uint8_t dayAlarm;
    uint8_t monthAlarm;
    uint8_t century;
    uint16_t iaPcBootArchitectureFlags;
    uint8_t reserved2;
    uint32_t flags;
    GenericAddressStructure resetReg;
    uint8_t resetValue;
    uint16_t armBootArchitectureFlags;
    uint8_t fadtMinorVersion;
    uint64_t xFirmwareControl;
    uint64_t xDsdt;
    GenericAddressStructure xPm1aEventBlock;
    GenericAddressStructure xPm1bEventBlock;
    GenericAddressStructure xPm1aControlBlock;
    GenericAddressStructure xPm1bControlBlock;
    GenericAddressStructure xPm2ControlBlock;
    GenericAddressStructure xPmTimerBlock;
    GenericAddressStructure xGpe0Block;
    GenericAddressStructure xGpe1Block;
    GenericAddressStructure sleepControlReg;
    GenericAddressStructure sleepStatusReg;
    uint64_t hypervisorVendorIdentity;
} __attribute__((packed));

static_assert(offsetof(Fadt, resetReg) == 116);
static_assert(offsetof(Fadt, xFirmwareControl) == 132);
static_assert(offsetof(Fadt, xDsdt) == 140);
static_assert(offsetof(Fadt, sleepControlReg) == 244);
static_assert(offsetof(Fadt, hypervisorVendorIdentity) == 268);
static_assert(sizeof(Fadt) == 276);

constexpr uint8_t kAcpiAddressSpaceSystemMemory = 0;
constexpr uint8_t kAcpiAddressSpaceSystemIo = 1;
constexpr uint32_t kFadtFlagHardwareReducedAcpi = 1U << 20;

// A resolved power-management register: either a memory-mapped location or an
// I/O port, produced by the FADT selectors below. Kept free of any I/O so the
// selection policy can be unit-tested on the host.
struct AcpiRegister {
    uint8_t addressSpace = 0;
    uint8_t bitWidth = 0;
    uint64_t address = 0;
    bool valid = false;
};

inline bool acpiSignatureEquals(const char* a, const char* b) {
    return strncmp(a, b, 4) == 0;
}

inline uint8_t acpiChecksum(const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint8_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum = static_cast<uint8_t>(sum + bytes[i]);
    }
    return sum;
}

inline bool acpiChecksumValid(const void* data, size_t length) {
    return data && length > 0 && acpiChecksum(data, length) == 0;
}

inline bool acpiRsdpValid(const Rsdp* rsdp) {
    if (!rsdp || memcmp(rsdp->signature, "RSD PTR ", 8) != 0) {
        return false;
    }
    if (!acpiChecksumValid(rsdp, sizeof(Rsdp))) {
        return false;
    }
    if (rsdp->revision < 2) {
        return true;
    }

    const Rsdp20* ext = reinterpret_cast<const Rsdp20*>(rsdp);
    if (ext->length < sizeof(Rsdp20) || ext->length > 4096) {
        return false;
    }
    return acpiChecksumValid(ext, ext->length);
}

inline bool acpiTableHeaderLooksValid(const AcpiHeader* header) {
    if (!header || header->length < sizeof(AcpiHeader)) {
        return false;
    }
    return true;
}

inline bool acpiTableValid(const void* table, const char* expectedSignature = nullptr) {
    const AcpiHeader* header = static_cast<const AcpiHeader*>(table);
    if (!acpiTableHeaderLooksValid(header)) {
        return false;
    }
    if (expectedSignature && !acpiSignatureEquals(header->signature, expectedSignature)) {
        return false;
    }
    return acpiChecksumValid(table, header->length);
}

inline bool acpiRootTableValid(const void* table, bool useXsdt) {
    return acpiTableValid(table, useXsdt ? "XSDT" : "RSDT");
}

inline size_t acpiRootEntryCount(const AcpiHeader* header, bool useXsdt) {
    if (!acpiTableHeaderLooksValid(header)) {
        return 0;
    }
    const size_t entrySize = useXsdt ? sizeof(uint64_t) : sizeof(uint32_t);
    return (header->length - sizeof(AcpiHeader)) / entrySize;
}

// A root-table entry must point at something that can plausibly hold an ACPI
// header. Reject the obvious sentinels (null and the all-ones value used by
// unmapped firmware regions) before any dereference; full structural and
// checksum validation still happens in acpiTableValid().
inline bool acpiEntryPointerPlausible(uint64_t physAddr) {
    if (physAddr == 0 || physAddr == UINT32_MAX || physAddr == UINT64_MAX) {
        return false;
    }
    return true;
}

inline bool fadtHasField(const Fadt* fadt, size_t offset, size_t length) {
    if (!fadt || !acpiTableHeaderLooksValid(&fadt->header)) {
        return false;
    }
    if (offset > UINT32_MAX - length) {
        return false;
    }
    return fadt->header.length >= offset + length;
}

inline bool fadtHasDsdt(const Fadt* fadt) {
    return fadtHasField(fadt, offsetof(Fadt, dsdt), sizeof(fadt->dsdt));
}

inline bool fadtHasXDsdt(const Fadt* fadt) {
    return fadtHasField(fadt, offsetof(Fadt, xDsdt), sizeof(fadt->xDsdt));
}

inline uint64_t fadtDsdtAddress(const Fadt* fadt) {
    if (!fadt) {
        return 0;
    }
    if (fadtHasXDsdt(fadt) && fadt->xDsdt) {
        return fadt->xDsdt;
    }
    if (fadtHasDsdt(fadt)) {
        return fadt->dsdt;
    }
    return 0;
}

inline bool acpiGasSupported(const GenericAddressStructure& gas) {
    return gas.address != 0 &&
           gas.bitOffset == 0 &&
           (gas.addressSpace == kAcpiAddressSpaceSystemMemory ||
            gas.addressSpace == kAcpiAddressSpaceSystemIo);
}

inline bool acpiGasSupported8(const GenericAddressStructure& gas) {
    return acpiGasSupported(gas) && gas.bitWidth == 8;
}

inline bool acpiGasSupported16(const GenericAddressStructure& gas) {
    return acpiGasSupported(gas) && (gas.bitWidth == 0 || gas.bitWidth >= 16);
}

inline AcpiRegister acpiRegisterFromGas(const GenericAddressStructure& gas) {
    AcpiRegister reg;
    reg.addressSpace = gas.addressSpace;
    reg.bitWidth = gas.bitWidth;
    reg.address = gas.address;
    reg.valid = true;
    return reg;
}

inline AcpiRegister acpiIoRegister(uint32_t port, uint8_t bitWidth) {
    AcpiRegister reg;
    if (!port) {
        return reg;
    }
    reg.addressSpace = kAcpiAddressSpaceSystemIo;
    reg.bitWidth = bitWidth;
    reg.address = port;
    reg.valid = true;
    return reg;
}

inline AcpiRegister fadtPm1aControlRegister(const Fadt* fadt) {
    if (!fadt) {
        return {};
    }
    if (fadtHasField(fadt, offsetof(Fadt, xPm1aControlBlock), sizeof(fadt->xPm1aControlBlock)) &&
        acpiGasSupported16(fadt->xPm1aControlBlock)) {
        return acpiRegisterFromGas(fadt->xPm1aControlBlock);
    }
    if (fadtHasField(fadt, offsetof(Fadt, pm1aControlBlock), sizeof(fadt->pm1aControlBlock))) {
        return acpiIoRegister(fadt->pm1aControlBlock, 16);
    }
    return {};
}

inline AcpiRegister fadtPm1bControlRegister(const Fadt* fadt) {
    if (!fadt) {
        return {};
    }
    if (fadtHasField(fadt, offsetof(Fadt, xPm1bControlBlock), sizeof(fadt->xPm1bControlBlock)) &&
        acpiGasSupported16(fadt->xPm1bControlBlock)) {
        return acpiRegisterFromGas(fadt->xPm1bControlBlock);
    }
    if (fadtHasField(fadt, offsetof(Fadt, pm1bControlBlock), sizeof(fadt->pm1bControlBlock))) {
        return acpiIoRegister(fadt->pm1bControlBlock, 16);
    }
    return {};
}

inline AcpiRegister fadtSleepControlRegister(const Fadt* fadt) {
    if (!fadt) {
        return {};
    }
    if (fadtHasField(fadt, offsetof(Fadt, sleepControlReg), sizeof(fadt->sleepControlReg)) &&
        acpiGasSupported8(fadt->sleepControlReg)) {
        return acpiRegisterFromGas(fadt->sleepControlReg);
    }
    return {};
}

inline AcpiRegister fadtResetRegister(const Fadt* fadt) {
    if (!fadt) {
        return {};
    }
    if (fadtHasField(fadt, offsetof(Fadt, resetReg), sizeof(fadt->resetReg)) &&
        acpiGasSupported8(fadt->resetReg)) {
        return acpiRegisterFromGas(fadt->resetReg);
    }
    return {};
}

// Prefer the 64-bit Generic Address Structure form of the ACPI PM timer when
// the FADT is long enough to contain it and it describes a usable block;
// otherwise fall back to the legacy 32-bit I/O port field. The PM timer is a
// 24/32-bit counter, so request a 32-bit-wide register.
inline AcpiRegister fadtPmTimerRegister(const Fadt* fadt) {
    if (!fadt) {
        return {};
    }
    if (fadtHasField(fadt, offsetof(Fadt, xPmTimerBlock), sizeof(fadt->xPmTimerBlock)) &&
        acpiGasSupported(fadt->xPmTimerBlock) &&
        (fadt->xPmTimerBlock.bitWidth == 0 || fadt->xPmTimerBlock.bitWidth >= 24)) {
        return acpiRegisterFromGas(fadt->xPmTimerBlock);
    }
    if (fadtHasField(fadt, offsetof(Fadt, pmTimerBlock), sizeof(fadt->pmTimerBlock))) {
        return acpiIoRegister(fadt->pmTimerBlock, 32);
    }
    return {};
}

// GPE (General Purpose Event) blocks drive wake/runtime events for devices such
// as USB controllers and GPIO-attached peripherals, so resolving them via the
// 64-bit GAS where available matters for future power-management work.
inline AcpiRegister fadtGpe0Register(const Fadt* fadt) {
    if (!fadt) {
        return {};
    }
    if (fadtHasField(fadt, offsetof(Fadt, xGpe0Block), sizeof(fadt->xGpe0Block)) &&
        acpiGasSupported(fadt->xGpe0Block)) {
        return acpiRegisterFromGas(fadt->xGpe0Block);
    }
    if (fadtHasField(fadt, offsetof(Fadt, gpe0Block), sizeof(fadt->gpe0Block)) &&
        fadt->gpe0Block) {
        return acpiIoRegister(fadt->gpe0Block, 8);
    }
    return {};
}

inline AcpiRegister fadtGpe1Register(const Fadt* fadt) {
    if (!fadt) {
        return {};
    }
    if (fadtHasField(fadt, offsetof(Fadt, xGpe1Block), sizeof(fadt->xGpe1Block)) &&
        acpiGasSupported(fadt->xGpe1Block)) {
        return acpiRegisterFromGas(fadt->xGpe1Block);
    }
    if (fadtHasField(fadt, offsetof(Fadt, gpe1Block), sizeof(fadt->gpe1Block)) &&
        fadt->gpe1Block) {
        return acpiIoRegister(fadt->gpe1Block, 8);
    }
    return {};
}

inline bool fadtHardwareReduced(const Fadt* fadt) {
    return fadt &&
           fadtHasField(fadt, offsetof(Fadt, flags), sizeof(fadt->flags)) &&
           (fadt->flags & kFadtFlagHardwareReducedAcpi) != 0;
}

} // namespace acpi
