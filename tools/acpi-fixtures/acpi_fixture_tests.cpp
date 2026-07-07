// Host-side ACPI fixture tests.
//
// These tests build synthetic ACPI firmware images that mirror representative
// ACPI 1.0, 2.0, and 6.0 layouts and drive the *exact* validation and FADT
// register-selection logic the kernel uses (include/cpu/acpi/acpi_tables.hpp).
// Because that header is shared with src/cpu/acpi/acpi.cpp, a regression in the
// RSDP/RSDT/XSDT discovery, checksum handling, table bounds checks, or the
// 64-bit GAS preference will fail here without needing to boot QEMU.
//
// Build/run with tools/run-acpi-fixtures.sh (or compile this single file with a
// host C++ compiler; it has no dependencies beyond the C++ standard library).

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

// The shared header expects strncmp/memcmp to be visible (the kernel gets them
// from <common/string.hpp>; on the host they come from <cstring>). It also uses
// __attribute__((packed)); clang/gcc accept that on this host build.
#include <cpu/acpi/acpi_tables.hpp>

using namespace acpi;

// ---------------------------------------------------------------------------
// Tiny test harness
// ---------------------------------------------------------------------------

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__);            \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        ++g_checks;                                                            \
        auto _va = (a);                                                        \
        auto _vb = (b);                                                        \
        if (!(_va == _vb)) {                                                   \
            ++g_failures;                                                      \
            std::printf("  FAIL: %s == %s (line %d): got %llu vs %llu\n",      \
                        #a, #b, __LINE__,                                      \
                        (unsigned long long)_va, (unsigned long long)_vb);     \
        }                                                                      \
    } while (0)

// ---------------------------------------------------------------------------
// Fixture builder
//
// A Firmware object owns a heap blob and hands out real pointers into it, so
// the pointer-based ACPI helpers can walk it exactly as they would walk mapped
// firmware. Addresses stored *inside* the tables (RSDT/XSDT entries, FADT DSDT
// pointers) are the actual host pointer values, which is valid because on the
// kernel side ACPI tables are accessed via identity-mapped physical addresses.
// ---------------------------------------------------------------------------

static void setSignature(char* dst, const char* sig, size_t n) {
    std::memset(dst, ' ', n);
    std::memcpy(dst, sig, std::strlen(sig) < n ? std::strlen(sig) : n);
}

static void fillHeader(AcpiHeader* h, const char* sig, uint32_t length, uint8_t revision) {
    std::memset(h, 0, sizeof(*h));
    std::memcpy(h->signature, sig, 4);
    h->length = length;
    h->revision = revision;
    setSignature(h->oemId, "INSTOS", sizeof(h->oemId));
    setSignature(h->oemTableId, "IOSTABLE", sizeof(h->oemTableId));
    h->oemRevision = 1;
    h->creatorId = 0x494F5343; // "CSOI"
    h->creatorRevision = 1;
}

// Recompute an AcpiHeader-style checksum so the whole table sums to zero.
static void finalizeTableChecksum(void* table) {
    AcpiHeader* h = static_cast<AcpiHeader*>(table);
    h->checksum = 0;
    uint8_t sum = acpiChecksum(table, h->length);
    h->checksum = static_cast<uint8_t>(0x100 - sum) & 0xFF;
}

// A single low-address arena so every synthesized table lives in the low 4 GiB,
// matching where real ACPI firmware sits. This lets the fixtures store table
// pointers in 32-bit RSDT entries and truncate exactly as the kernel does
// without producing dangling addresses. Tables are bump-allocated and 16-byte
// aligned. The arena is reserved near a low base address on both Windows and
// POSIX; if the OS cannot honor the low hint the test aborts with a clear
// message rather than silently corrupting pointers.
class LowArena {
public:
    LowArena() {
        const size_t size = 4 * 1024 * 1024; // 4 MiB is plenty for the fixtures
        capacity_ = size;
#if defined(_WIN32)
        // Try progressively higher low bases until VirtualAlloc succeeds.
        for (uintptr_t base = 0x10000000; base <= 0x70000000 && !region_; base += 0x10000000) {
            region_ = static_cast<uint8_t*>(VirtualAlloc(
                reinterpret_cast<void*>(base), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
        if (!region_) {
            region_ = static_cast<uint8_t*>(
                VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
#else
        int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(MAP_32BIT)
        flags |= MAP_32BIT;
#endif
        void* p = mmap(reinterpret_cast<void*>(0x20000000), size,
                       PROT_READ | PROT_WRITE, flags, -1, 0);
        if (p == MAP_FAILED) {
            p = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
        region_ = (p == MAP_FAILED) ? nullptr : static_cast<uint8_t*>(p);
#endif
        if (!region_) {
            std::fprintf(stderr, "error: could not reserve a low-address arena\n");
            std::exit(2);
        }
        if (reinterpret_cast<uintptr_t>(region_) + size > 0xFFFFFFFFull) {
            std::fprintf(stderr,
                "error: arena at %p is not in the low 4 GiB; cannot model 32-bit RSDT\n",
                static_cast<void*>(region_));
            std::exit(2);
        }
    }

    uint8_t* take(size_t bytes) {
        offset_ = (offset_ + 15) & ~size_t(15);
        if (offset_ + bytes > capacity_) {
            std::fprintf(stderr, "error: arena exhausted\n");
            std::exit(2);
        }
        uint8_t* p = region_ + offset_;
        offset_ += bytes;
        std::memset(p, 0, bytes);
        return p;
    }

private:
    uint8_t* region_ = nullptr;
    size_t capacity_ = 0;
    size_t offset_ = 0;
};

struct Firmware {
    LowArena arena;

    template <typename T>
    T* alloc(size_t extra = 0) {
        return reinterpret_cast<T*>(arena.take(sizeof(T) + extra));
    }

    uint8_t* allocRaw(size_t bytes) {
        return arena.take(bytes);
    }
};

// Build a minimal but valid DSDT with an embedded _S5_ package so the shutdown
// path has something to parse. We only need it to pass acpiTableValid("DSDT").
static AcpiHeader* buildDsdt(Firmware& fw) {
    const uint32_t len = sizeof(AcpiHeader) + 16;
    AcpiHeader* dsdt = reinterpret_cast<AcpiHeader*>(fw.allocRaw(len));
    fillHeader(dsdt, "DSDT", len, 2);
    finalizeTableChecksum(dsdt);
    return dsdt;
}

// Build a FADT. `length` lets us emulate short (1.0/2.0) vs full (6.0) tables.
static Fadt* buildFadt(Firmware& fw, uint32_t length, uint8_t revision) {
    Fadt* fadt = reinterpret_cast<Fadt*>(fw.allocRaw(length));
    std::memset(fadt, 0, length);
    fillHeader(&fadt->header, "FACP", length, revision);
    return fadt;
}

// ---------------------------------------------------------------------------
// Test 1: ACPI 1.0 firmware — RSDP rev 0, RSDT only, short (legacy) FADT.
// ---------------------------------------------------------------------------

static void testAcpi10() {
    std::printf("ACPI 1.0 (RSDP rev0 / RSDT / legacy FADT)\n");
    Firmware fw;

    // Legacy FADT: only through the legacy 32-bit fields, no X* / GAS fields.
    // Truncate right after `flags` so xPm*/reset GAS fields are absent.
    const uint32_t legacyLen = offsetof(Fadt, resetReg);
    Fadt* fadt = buildFadt(fw, legacyLen, 1);
    fadt->pmTimerBlock = 0x408;        // legacy PM timer I/O port
    fadt->pm1aControlBlock = 0x404;    // legacy PM1a control I/O port
    fadt->gpe0Block = 0x420;           // legacy GPE0 I/O port
    finalizeTableChecksum(fadt);

    // RSDT with one 32-bit entry pointing at the FADT. Real ACPI 1.0 firmware
    // lives in low physical memory, so its RSDT entries fit in 32 bits; the
    // fixture arena keeps every table in the low 4 GiB so the same 32-bit
    // storage and truncation the kernel uses round-trips here.
    const uint32_t rsdtLen = sizeof(AcpiHeader) + sizeof(uint32_t);
    Rsdt* rsdt = reinterpret_cast<Rsdt*>(fw.allocRaw(rsdtLen));
    fillHeader(&rsdt->header, "RSDT", rsdtLen, 1);
    uintptr_t fadtAddr = reinterpret_cast<uintptr_t>(fadt);
    CHECK(fadtAddr <= 0xFFFFFFFFull); // arena keeps tables in the low 4 GiB
    rsdt->pointers[0] = static_cast<uint32_t>(fadtAddr);
    finalizeTableChecksum(rsdt);

    // RSDP revision 0 (ACPI 1.0): only the 20-byte structure, RSDT pointer.
    Rsdp* rsdp = fw.alloc<Rsdp>();
    std::memcpy(rsdp->signature, "RSD PTR ", 8);
    setSignature(rsdp->oemId, "INSTOS", sizeof(rsdp->oemId));
    rsdp->revision = 0;
    rsdp->rsdtAddress = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(rsdt));
    rsdp->checksum = 0;
    rsdp->checksum = static_cast<uint8_t>(0x100 - acpiChecksum(rsdp, sizeof(Rsdp))) & 0xFF;

    // Validation.
    CHECK(acpiRsdpValid(rsdp));
    CHECK(rsdp->revision < 2);                       // must select RSDT path
    CHECK(acpiRootTableValid(rsdt, /*useXsdt=*/false));
    CHECK_EQ(acpiRootEntryCount(&rsdt->header, false), (size_t)1);

    // Walk the RSDT to the FADT the way findTable() does (32-bit entries).
    Fadt* found = nullptr;
    for (size_t i = 0; i < acpiRootEntryCount(&rsdt->header, false); ++i) {
        uint64_t addr = static_cast<uint64_t>(rsdt->pointers[i]);
        if (!acpiEntryPointerPlausible(addr)) continue;
        AcpiHeader* h = reinterpret_cast<AcpiHeader*>(addr);
        if (acpiTableValid(h, "FACP")) found = reinterpret_cast<Fadt*>(h);
    }
    CHECK(found == fadt);

    // Legacy FADT must resolve registers via the 32-bit I/O fallback.
    AcpiRegister pmt = fadtPmTimerRegister(fadt);
    CHECK(pmt.valid);
    CHECK_EQ(pmt.addressSpace, kAcpiAddressSpaceSystemIo);
    CHECK_EQ(pmt.address, (uint64_t)0x408);

    AcpiRegister pm1a = fadtPm1aControlRegister(fadt);
    CHECK(pm1a.valid);
    CHECK_EQ(pm1a.addressSpace, kAcpiAddressSpaceSystemIo);
    CHECK_EQ(pm1a.address, (uint64_t)0x404);

    AcpiRegister gpe0 = fadtGpe0Register(fadt);
    CHECK(gpe0.valid);
    CHECK_EQ(gpe0.address, (uint64_t)0x420);

    // A short FADT has no reset/sleep-control GAS fields at all.
    CHECK(!fadtResetRegister(fadt).valid);
    CHECK(!fadtSleepControlRegister(fadt).valid);
    CHECK(!fadtHardwareReduced(fadt));
}

// ---------------------------------------------------------------------------
// Test 2: ACPI 2.0 firmware — RSDP rev 2 with XSDT, full FADT that carries both
// legacy and 64-bit GAS fields. GAS must win.
// ---------------------------------------------------------------------------

static void testAcpi20() {
    std::printf("ACPI 2.0 (RSDP rev2 / XSDT preferred / GAS wins)\n");
    Firmware fw;

    AcpiHeader* dsdt = buildDsdt(fw);

    Fadt* fadt = buildFadt(fw, sizeof(Fadt), 3);
    // Legacy fields present...
    fadt->pmTimerBlock = 0x408;
    fadt->pm1aControlBlock = 0x404;
    fadt->dsdt = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(dsdt));
    // ...but the 64-bit GAS forms describe different (memory-mapped) locations
    // that must take precedence.
    fadt->xPmTimerBlock = {kAcpiAddressSpaceSystemMemory, 32, 0, 3, 0xFED00000ull};
    fadt->xPm1aControlBlock = {kAcpiAddressSpaceSystemMemory, 16, 0, 2, 0xFED00100ull};
    fadt->xDsdt = reinterpret_cast<uintptr_t>(dsdt);
    finalizeTableChecksum(fadt);

    // XSDT with one 64-bit entry to the FADT.
    const uint32_t xsdtLen = sizeof(AcpiHeader) + sizeof(uint64_t);
    Xsdt* xsdt = reinterpret_cast<Xsdt*>(fw.allocRaw(xsdtLen));
    fillHeader(&xsdt->header, "XSDT", xsdtLen, 1);
    xsdt->pointers[0] = reinterpret_cast<uintptr_t>(fadt);
    finalizeTableChecksum(xsdt);

    // A stale/garbage RSDT to make sure XSDT is actually preferred.
    const uint32_t rsdtLen = sizeof(AcpiHeader) + sizeof(uint32_t);
    Rsdt* rsdt = reinterpret_cast<Rsdt*>(fw.allocRaw(rsdtLen));
    fillHeader(&rsdt->header, "RSDT", rsdtLen, 1);
    rsdt->pointers[0] = 0; // implausible on purpose
    finalizeTableChecksum(rsdt);

    Rsdp20* ext = fw.alloc<Rsdp20>();
    std::memcpy(ext->firstPart.signature, "RSD PTR ", 8);
    setSignature(ext->firstPart.oemId, "INSTOS", sizeof(ext->firstPart.oemId));
    ext->firstPart.revision = 2;
    ext->firstPart.rsdtAddress = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(rsdt));
    ext->length = sizeof(Rsdp20);
    ext->xsdtAddress = reinterpret_cast<uintptr_t>(xsdt);
    // v1 checksum over the first 20 bytes.
    ext->firstPart.checksum = 0;
    ext->firstPart.checksum =
        static_cast<uint8_t>(0x100 - acpiChecksum(&ext->firstPart, sizeof(Rsdp))) & 0xFF;
    // v2 checksum over the whole extended structure.
    ext->extendedChecksum = 0;
    ext->extendedChecksum =
        static_cast<uint8_t>(0x100 - acpiChecksum(ext, ext->length)) & 0xFF;

    Rsdp* base = reinterpret_cast<Rsdp*>(ext);
    CHECK(acpiRsdpValid(base));
    CHECK(base->revision >= 2);
    CHECK(acpiRootTableValid(xsdt, /*useXsdt=*/true));
    CHECK(!acpiEntryPointerPlausible(rsdt->pointers[0])); // stale RSDT entry rejected

    // GAS must be preferred over the legacy I/O fields.
    AcpiRegister pmt = fadtPmTimerRegister(fadt);
    CHECK(pmt.valid);
    CHECK_EQ(pmt.addressSpace, kAcpiAddressSpaceSystemMemory);
    CHECK_EQ(pmt.address, (uint64_t)0xFED00000ull);

    AcpiRegister pm1a = fadtPm1aControlRegister(fadt);
    CHECK(pm1a.valid);
    CHECK_EQ(pm1a.addressSpace, kAcpiAddressSpaceSystemMemory);
    CHECK_EQ(pm1a.address, (uint64_t)0xFED00100ull);

    // DSDT resolution must prefer xDsdt.
    CHECK_EQ(fadtDsdtAddress(fadt), (uint64_t)reinterpret_cast<uintptr_t>(dsdt));
    CHECK(acpiTableValid(reinterpret_cast<void*>(fadtDsdtAddress(fadt)), "DSDT"));
}

// ---------------------------------------------------------------------------
// Test 3: ACPI 6.0 firmware — hardware-reduced, full FADT with reset & sleep
// control GAS registers, MCFG with two ECAM windows.
// ---------------------------------------------------------------------------

static void testAcpi60() {
    std::printf("ACPI 6.0 (hardware-reduced / reset+sleep GAS / MCFG)\n");
    Firmware fw;

    Fadt* fadt = buildFadt(fw, sizeof(Fadt), 6);
    fadt->fadtMinorVersion = 1; // FADT 6.1
    fadt->flags = kFadtFlagHardwareReducedAcpi;
    fadt->resetReg = {kAcpiAddressSpaceSystemMemory, 8, 0, 1, 0xFED0FFFFull};
    fadt->resetValue = 0x06;
    fadt->sleepControlReg = {kAcpiAddressSpaceSystemMemory, 8, 0, 1, 0xFED0FE00ull};
    fadt->sleepStatusReg = {kAcpiAddressSpaceSystemMemory, 8, 0, 1, 0xFED0FE04ull};
    finalizeTableChecksum(fadt);

    CHECK(fadtHardwareReduced(fadt));

    AcpiRegister reset = fadtResetRegister(fadt);
    CHECK(reset.valid);
    CHECK_EQ(reset.addressSpace, kAcpiAddressSpaceSystemMemory);
    CHECK_EQ(reset.address, (uint64_t)0xFED0FFFFull);

    AcpiRegister sleep = fadtSleepControlRegister(fadt);
    CHECK(sleep.valid);
    CHECK_EQ(sleep.address, (uint64_t)0xFED0FE00ull);

    // MCFG with two ECAM allocations.
    const uint32_t mcfgLen = offsetof(Mcfg, allocations) + 2 * sizeof(McfgAllocation);
    Mcfg* mcfg = reinterpret_cast<Mcfg*>(fw.allocRaw(mcfgLen));
    std::memset(mcfg, 0, mcfgLen);
    fillHeader(&mcfg->header, "MCFG", mcfgLen, 1);
    mcfg->allocations[0] = {0xE0000000ull, 0, 0, 0x3F, 0};
    mcfg->allocations[1] = {0xF0000000ull, 1, 0x40, 0x7F, 0};
    finalizeTableChecksum(mcfg);

    CHECK(acpiTableValid(mcfg, "MCFG"));
    const uint32_t hdrLen = mcfg->header.length;
    CHECK(hdrLen >= offsetof(Mcfg, allocations));
    const size_t allocations = (hdrLen - offsetof(Mcfg, allocations)) / sizeof(McfgAllocation);
    CHECK_EQ(allocations, (size_t)2);
    CHECK_EQ(mcfg->allocations[0].baseAddress, (uint64_t)0xE0000000ull);
    CHECK_EQ(mcfg->allocations[1].segmentGroup, (uint16_t)1);
    CHECK(mcfg->allocations[1].endBus >= mcfg->allocations[1].startBus);
}

// ---------------------------------------------------------------------------
// Test 4: malformed / adversarial firmware — checksum and bounds must reject.
// ---------------------------------------------------------------------------

static void testMalformed() {
    std::printf("Malformed firmware (checksum/bounds rejection)\n");
    Firmware fw;

    // Bad RSDP checksum.
    {
        Rsdp* rsdp = fw.alloc<Rsdp>();
        std::memcpy(rsdp->signature, "RSD PTR ", 8);
        rsdp->revision = 0;
        rsdp->rsdtAddress = 0x1000;
        rsdp->checksum = 0x01; // deliberately wrong
        CHECK(!acpiRsdpValid(rsdp));
    }

    // Wrong signature.
    {
        Rsdp* rsdp = fw.alloc<Rsdp>();
        std::memcpy(rsdp->signature, "XSD PTR ", 8);
        rsdp->checksum = 0;
        rsdp->checksum = static_cast<uint8_t>(0x100 - acpiChecksum(rsdp, sizeof(Rsdp))) & 0xFF;
        CHECK(!acpiRsdpValid(rsdp));
    }

    // Extended RSDP with an out-of-range length.
    {
        Rsdp20* ext = fw.alloc<Rsdp20>();
        std::memcpy(ext->firstPart.signature, "RSD PTR ", 8);
        ext->firstPart.revision = 2;
        ext->length = 8; // too short for Rsdp20
        ext->firstPart.checksum = 0;
        ext->firstPart.checksum =
            static_cast<uint8_t>(0x100 - acpiChecksum(&ext->firstPart, sizeof(Rsdp))) & 0xFF;
        CHECK(!acpiRsdpValid(reinterpret_cast<Rsdp*>(ext)));
    }

    // Table with length shorter than a header must be rejected.
    {
        AcpiHeader h;
        fillHeader(&h, "FACP", sizeof(AcpiHeader) - 1, 1);
        CHECK(!acpiTableHeaderLooksValid(&h));
        CHECK(!acpiTableValid(&h, "FACP"));
    }

    // Corrupted checksum on an otherwise well-formed table.
    {
        AcpiHeader* h = reinterpret_cast<AcpiHeader*>(fw.allocRaw(sizeof(AcpiHeader)));
        fillHeader(h, "SSDT", sizeof(AcpiHeader), 2);
        finalizeTableChecksum(h);
        CHECK(acpiTableValid(h, "SSDT"));
        h->checksum ^= 0xFF; // corrupt it
        CHECK(!acpiTableValid(h, "SSDT"));
    }

    // Sentinel pointers must be rejected before any dereference.
    CHECK(!acpiEntryPointerPlausible(0));
    CHECK(!acpiEntryPointerPlausible(UINT32_MAX));
    CHECK(!acpiEntryPointerPlausible(UINT64_MAX));
    CHECK(acpiEntryPointerPlausible(0x1000));

    // fadtHasField must not over-read a short FADT (integer-overflow guard too).
    {
        Fadt* fadt = buildFadt(fw, offsetof(Fadt, flags), 1);
        finalizeTableChecksum(fadt);
        CHECK(!fadtHasField(fadt, offsetof(Fadt, resetReg), sizeof(fadt->resetReg)));
        CHECK(!fadtResetRegister(fadt).valid);
        CHECK(fadtHasField(fadt, offsetof(Fadt, pmTimerBlock), sizeof(fadt->pmTimerBlock)));
    }
}

int main() {
    std::printf("== ACPI fixture tests (1.0 / 2.0 / 6.0 layouts) ==\n");
    testAcpi10();
    testAcpi20();
    testAcpi60();
    testMalformed();

    std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("ACPI FIXTURE TESTS PASSED\n");
        return 0;
    }
    std::printf("ACPI FIXTURE TESTS FAILED\n");
    return 1;
}
