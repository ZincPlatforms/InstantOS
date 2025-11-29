#include "bitmap.hpp"
#include <string.h>

Bitmap::Bitmap() : bmpBuffer(nullptr), bmpSize(0) {}
Bitmap::Bitmap(uint8_t* buffer, size_t size) : bmpBuffer(nullptr), bmpSize(0) {
    init(buffer, size);
}

void Bitmap::init(uint8_t* buffer, size_t size) {
    bmpBuffer = buffer;
    bmpSize = size;

    memset(bmpBuffer, 0xFF, (size + 7) / 8);
}

bool Bitmap::set(size_t index) {
    if (index >= bmpSize) return false;

    size_t byte = index / 8;
    size_t bit = index % 8;
    bmpBuffer[byte] |= (1 << bit);
    return true;
}

bool Bitmap::clear(size_t index) {
    if (index >= bmpSize) return false;

    size_t byte = index / 8;
    size_t bit = index % 8;
    bmpBuffer[byte] &= ~(1 << bit);
    return true;
}

bool Bitmap::get(size_t index) const {
    if (index >= bmpSize) return false;

    size_t byte = index / 8;
    size_t bit = index % 8;
    return (bmpBuffer[byte] & (1 << bit)) != 0;
}

size_t Bitmap::findFirstFree() const {
    for (size_t i = 0; i < bmpSize; i++) {
        if (!get(i)) return i;
    }
    return bmpSize;
}
    
size_t Bitmap::findFirstFreeRegion(size_t count) const {
    if (count == 0) return bmpSize;

    size_t consecutive = 0;
    size_t start = 0;
    
    for (size_t i = 0; i < bmpSize; i++) {
        if (!get(i)) {
            if (consecutive == 0) start = i;
            consecutive++;

            if (consecutive == count) return start;
        } else {
            consecutive = 0;
        }
    }

    return bmpSize;
}
    
bool Bitmap::setRange(size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!set(start + i)) return false;
    }
    return true;
}

bool Bitmap::clearRange(size_t start, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (!clear(start + i)) return false;
    }
    return true;
}