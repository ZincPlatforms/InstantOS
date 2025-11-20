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
public:
    Console(Framebuffer* framebufferVal);

    void drawChar(const char c);
    void drawText(const char* str);
    void setTextColor(Color color);
};