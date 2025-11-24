#include <stdint.h>

extern "C" void reloadSegments(uint16_t data, uint16_t code);

enum class SegmentSelectors : uint16_t {
    KernelCode = 0x08,
    KernelData = 0x10,
    UserCode   = 0x18,
    UserData   = 0x20,
    TaskState  = 0x28
};

struct GDTEntry {
    uint16_t limitLow;
    uint16_t baseLow;
    uint8_t baseMiddle;
    uint8_t access;
    uint8_t granularity;
    uint8_t baseHigh;
} __attribute__((packed));

struct GDTPointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct TSSEntry {
    uint16_t limitLow;
    uint16_t baseLow;
    uint8_t  baseMiddle1;
    uint8_t  access;
    uint8_t  limitHigh : 4;
    uint8_t  flags : 4;
    uint8_t  baseMiddle2;

    uint32_t baseHigh;
    uint32_t reserved;
} __attribute__((packed));

class GDT {
private:
    GDTEntry gdt[8];
    GDTPointer gdtp;
    TSSEntry tss;
    
public:
    GDT();

    void setTSS(int index, uint64_t base, uint32_t limit);
    void setGate64(int num, uint8_t access, uint8_t gran);
    void setGate32(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran);

    ~GDT();
};