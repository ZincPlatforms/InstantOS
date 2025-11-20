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
    
    private:
        uint32_t* address;
        uint64_t width;
        uint64_t height;
        uint64_t pitch;
};