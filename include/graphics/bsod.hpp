#pragma once

#include <stddef.h>
#include <stdint.h>

struct DrawPixel {
    uint32_t amountOf;
    uint32_t pixel;
};

extern const DrawPixel bsod[];
extern const size_t bsodPixelCount;
