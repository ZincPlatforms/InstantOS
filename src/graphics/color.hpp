#pragma once

#include <stdint.h>

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;

    Color() : r(0), g(0), b(0) {}
    Color(uint32_t rgb) {
        r = (rgb >> 16) & 0xFF;
        g = (rgb >> 8) & 0xFF;  
        b = rgb & 0xFF;
    }
    Color(uint8_t r, uint8_t g, uint8_t b) 
    : r(r), g(g), b(b) {}

    operator uint32_t() const {
        return (static_cast<uint32_t>(r) << 16) |
                (static_cast<uint32_t>(g) << 8) |
                (static_cast<uint32_t>(b));
    }
};