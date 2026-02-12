#pragma once

#include <stdint.h>
#include "color.hpp"
#include "buffer.hpp"

class Framebuffer {
    public:
        Framebuffer();
        ~Framebuffer();

        void putPixel(uint64_t x, uint64_t y, Color color);
        void clear(Color color);
        uint64_t getWidth();
        uint64_t getHeight();
        Color getPixel(uint64_t x, uint64_t y);
        void* getRaw();
        uint64_t getPitch();

        uint8_t getRedMaskSize() { return buffer->getRedMaskSize(); }
        uint8_t getRedMaskShift() { return buffer->getRedMaskShift(); }
        uint8_t getGreenMaskSize() { return buffer->getGreenMaskSize(); }
        uint8_t getGreenMaskShift() { return buffer->getGreenMaskShift(); }
        uint8_t getBlueMaskSize() { return buffer->getBlueMaskSize(); }
        uint8_t getBlueMaskShift() { return buffer->getBlueMaskShift(); }

    private:
        Buffer* buffer;
};