#include "console.hpp"
#include "font.hpp"
#include <memory.h>

Console::Console(Framebuffer* framebufferVal){
    framebuffer = framebufferVal;
    posX = 0;
    posY = 0;
}

void Console::toString(char* ptr, int64_t num, int radix){
    char* ptr1 = ptr;
    char tmp_char;
    int tmp_value;
    bool negative = false;

    if (num == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        drawText(ptr);
    }

    if (num < 0 && radix == 10) {
        negative = true;
        num = -num;
    }

    while (num != 0) {
        tmp_value = num % radix;
        num /= radix;
        
        if (tmp_value < 10)
            *ptr++ = "0123456789"[tmp_value];
        else
            *ptr++ = "abcdefghijklmnopqrstuvwxyz"[tmp_value - 10];
    }

    if (negative)
        *ptr++ = '-';

    *ptr = '\0';

    ptr--;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

void Console::toString(char* ptr, uint64_t num, int radix){
    char* ptr1 = ptr;
    char tmp_char;
    int tmp_value;

    if (num == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        drawText(ptr);
    }

    while (num != 0) {
        tmp_value = num % radix;
        num /= radix;
        
        if (tmp_value < 10)
            *ptr++ = "0123456789"[tmp_value];
        else
            *ptr++ = "abcdefghijklmnopqrstuvwxyz"[tmp_value - 10];
    }

    *ptr = '\0';

    ptr--;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

void Console::drawChar(const char c){
    if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7F) return;

    const uint8_t* glyph = font_8x16[c - 0x20];

    uint32_t baseX = posX;
    uint32_t baseY = posY;

    for (uint64_t y = 0; y < 16; y++) {
        uint8_t data = glyph[y];
        for (uint64_t x = 0; x < 8; x++) {
            if (data & (0x80 >> x)) {
                uint64_t locX = baseX + x;
                uint64_t locY = baseY + y;
            
                if (locX < framebuffer->getWidth() && locY < framebuffer->getHeight()) {
                    framebuffer->putPixel(locX, locY, drawColor);
                }
            } else {
                uint64_t locX = baseX + x;
                uint64_t locY = baseY + y;

                auto background = framebuffer->getPixel(locX, locY + 16);
                if (locX < framebuffer->getWidth() && locY < framebuffer->getHeight()) {
                    framebuffer->putPixel(locX, locY, background);
                }
            }
        }
    }
}

void Console::drawNumber(int64_t str){
    drawNumber(str);
}
void Console::drawHex(uint64_t str){
    drawNumber(str);
}

void Console::advance() {
    posX += 8; // character width
    if (posX >= framebuffer->getWidth()) {
        newLine();
    }
}

void Console::newLine() {
    posX = 0;
    posY += 16; // char height
    
    if(shouldScroll()){
        scroll();
    }
}

bool Console::shouldScroll(){
    return posY >= framebuffer->getHeight();
}

void Console::scroll(){
    uint64_t scrollHeight = 16;
    uint64_t LineBytes = framebuffer->getWidth() * sizeof(uint32_t);
    uint32_t* fb = (uint32_t*)framebuffer->getRaw();
    Framebuffer* temp = framebuffer;

    auto getMemAddress = [temp, fb](uint32_t x, uint32_t y) -> uint8_t* {
        return reinterpret_cast<uint8_t*>(&fb[y * temp->getPitch() + x]);
    };

    for (uint64_t y = scrollHeight; y < framebuffer->getHeight(); y++) {
        auto dest = getMemAddress(0, y - scrollHeight);
        auto src = getMemAddress(0, y);
        memcpy(dest, src, LineBytes);
    }
        
    for (uint64_t y = framebuffer->getHeight() - scrollHeight; y < framebuffer->getHeight(); y++) {
        for (uint64_t x = 0; x < framebuffer->getWidth(); x++) {
            framebuffer->putPixel(x, y, 0x00ff00);
        }
    }
        
    posY = framebuffer->getHeight() - scrollHeight;
}

void Console::drawText(const char* str){
    while (*str) {
        if(*str == '\n') {
            newLine();
            str++;
        } else if(*str == '\t') {
            drawText("    ");
            str++;
        } else if(*str == '\r') {
            posX = 0;
            str++;
        } else if(*str == '\b') {
            posX -= 8;
            drawChar(' ');
            str++;
        } else {
            drawChar(*str++);
            advance();
        }
    }
}

void Console::setTextColor(Color color){
    drawColor = color;
}