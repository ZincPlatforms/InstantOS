#include "gdt.hpp"

GDT::GDT(){
    gdtp.limit = (sizeof(GDTEntry) * 8) - 1;
    gdtp.base = (uint64_t)&gdt;
    
    setGate32(0, 0, 0, 0, 0);
    
    setGate64(1, 0x9A, 0x20);
    setGate64(2, 0x92, 0x00);
    
    setGate64(3, 0xFA, 0x20);
    setGate64(4, 0xF2, 0x00);
    
    uint64_t tssBase = (uint64_t)&tss;
    uint32_t tssLimit = sizeof(TSSEntry) - 1;
    
    setTSS(5, tssBase, tssLimit);
    
    asm volatile("lgdt %0" : : "m"(gdtp));
    
    reloadSegments((uint16_t)SegmentSelectors::KernelData, (uint16_t)SegmentSelectors::KernelCode);

    asm volatile("ltr %0" : : "r"(SegmentSelectors::TaskState));
}

void GDT::setGate64(int num, uint8_t access, uint8_t gran){
    gdt[num].limitLow = 0;
    gdt[num].baseLow = 0;
    gdt[num].baseMiddle = 0;
    gdt[num].baseHigh = 0;
    gdt[num].access = access;
    gdt[num].granularity = gran;
}

void GDT::setGate32(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran){
    gdt[num].baseLow = (base & 0xFFFF);
    gdt[num].baseMiddle = (base >> 16) & 0xFF;
    gdt[num].baseHigh = (base >> 24) & 0xFF;
    
    gdt[num].limitLow = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);
    
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

void GDT::setTSS(int index, uint64_t base, uint32_t limit) {
    gdt[index].limitLow       = limit & 0xFFFF;
    gdt[index].baseLow        = base & 0xFFFF;
    gdt[index].baseMiddle     = (base >> 16) & 0xFF;
    gdt[index].access         = 0x89;
    gdt[index].granularity    = ((limit >> 16) & 0x0F);
    gdt[index].baseHigh       = (base >> 24) & 0xFF;

    uint32_t base_high32 = (base >> 32) & 0xFFFFFFFF;
    gdt[index + 1].limitLow       = base_high32 & 0xFFFF;
    gdt[index + 1].baseLow        = (base_high32 >> 16) & 0xFFFF;

    gdt[index + 1].baseMiddle = 0;
    gdt[index + 1].baseHigh   = 0;
    gdt[index + 1].access     = 0;
    gdt[index + 1].granularity = 0;
}

GDT::~GDT(){

}