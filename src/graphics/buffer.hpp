#pragma once

#include <stdint.h>
#include "color.hpp"
#include <limine.h>

class Buffer {
    public:
        Buffer(limine_framebuffer* fb);
        ~Buffer();

        void putPixel(uint64_t x, uint64_t y, Color color);
        void clear(Color color);
        uint64_t getWidth();
        uint64_t getHeight();
        Color getPixel(uint64_t x, uint64_t y);
        void* getRaw();
        uint64_t getPitch();

        uint8_t getRedMaskSize() { return red_mask_size; }
        uint8_t getRedMaskShift() { return red_mask_shift; }
        uint8_t getGreenMaskSize() { return green_mask_size; }
        uint8_t getGreenMaskShift() { return green_mask_shift; }
        uint8_t getBlueMaskSize() { return blue_mask_size; }
        uint8_t getBlueMaskShift() { return blue_mask_shift; }
    private:
        uint32_t* address;
        uint64_t width;
        uint64_t height;
        uint64_t pitch;
        uint8_t red_mask_size, red_mask_shift, green_mask_size, green_mask_shift, blue_mask_size, blue_mask_shift;
};