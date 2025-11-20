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

    private:
        Buffer* buffer;
};