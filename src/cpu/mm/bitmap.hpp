#pragma once

#include <cstdint>
#include <cstddef>

class Bitmap {
public:
    Bitmap();
    Bitmap(uint8_t* buffer, size_t size);
    void init(uint8_t* buffer, size_t size);
    bool get(size_t index) const;
    bool set(size_t index);
    
    bool clear(size_t index);
    
    size_t findFirstFree() const;
    size_t findFirstFreeRegion(size_t count) const;
    
    bool setRange(size_t start, size_t count);
    bool clearRange(size_t start, size_t count);
    
    size_t size() const { return bmpSize; }
    
private:
    uint8_t* bmpBuffer;
    size_t bmpSize;
};
