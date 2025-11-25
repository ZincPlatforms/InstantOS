#pragma once

#include <cstdint>
#include "framebuffer.hpp"

class Console {
private:
    Framebuffer* framebuffer;
    uint32_t posX, posY;
    Color drawColor;
    void advance();
    void newLine();

    bool shouldScroll();
    void scroll();

    void toString(char* ptr, int64_t num, int radix);
    void toString(char* ptr, uint64_t num, int radix);
public:
    Console(Framebuffer* framebufferVal);

    void drawChar(const char c);
    void drawText(const char* str);
    void drawNumber(int64_t str);
    void drawHex(uint64_t str);
    void setTextColor(Color color);
};

extern Console* console;