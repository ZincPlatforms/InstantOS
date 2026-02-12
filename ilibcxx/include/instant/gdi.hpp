#pragma once
#include "syscall.hpp"
#include "../string.hpp"
#include "./font.hpp"
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

extern "C" void* memset32(void* dest, uint32_t value, uint64_t count);
inline int abs(int x) { return x < 0 ? -x : x; }

namespace instant {
    struct Color {
        unsigned char r, g, b;
        
        Color(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}
        
        uint32_t toBGR() const {
            return (b << 16) | (g << 8) | r;
        }
        
        uint32_t toRGB() const {
            return (r << 16) | (g << 8) | b;
        }
    };

    struct FBInfo {
        uint64_t addr;
        uint32_t width;
        uint32_t height;
        uint32_t pitch;
        uint16_t bpp;
        uint8_t redMaskSize;
        uint8_t redMaskShift;
        uint8_t greenMaskSize;
        uint8_t greenMaskShift;
        uint8_t blueMaskSize;
        uint8_t blueMaskShift;
    };

    class GDI {
        public:
            static constexpr uint32_t MAX_BACKBUFFER_SIZE = 1920 * 1080;
            
            static GDI& getContext() {
                static GDI gdi;
                return gdi;
            }

            bool initialize() {
                if (isInitialized) return true;
                
                FBInfo fbInfo;
                fbInfo.addr = 0;
                fbInfo.width = 0;
                fbInfo.height = 0;
                fbInfo.pitch = 0;
                fbInfo.bpp = 0;                
                
                long result = sys::syscall1(sys::Syscall::FramebufferInfo, reinterpret_cast<long>(&fbInfo));
                if (result != 0) return false;
                
                if (fbInfo.width == 0 || fbInfo.height == 0 || fbInfo.pitch == 0 || fbInfo.addr == 0) {
                    return false;
                }
                
                frontbuf = reinterpret_cast<void*>(fbInfo.addr);
                width = fbInfo.width;
                height = fbInfo.height;
                pitch = fbInfo.pitch;
                bpp = fbInfo.bpp;
                redMaskShift = fbInfo.redMaskShift;
                greenMaskShift = fbInfo.greenMaskShift;
                blueMaskShift = fbInfo.blueMaskShift;
                
                redMaskSize = fbInfo.redMaskSize;
                greenMaskSize = fbInfo.greenMaskSize;
                blueMaskSize = fbInfo.blueMaskSize;
                
                if (width * height > MAX_BACKBUFFER_SIZE) {
                    return false;
                }
                
                isInitialized = true;
                return true;
            }

            bool isReady() const { return isInitialized; }

            void drawLine(int x0, int y0, int x1, int y1, Color col) {
                int dx = abs(x1 - x0);
                int dy = abs(y1 - y0);
                int sx = x0 < x1 ? 1 : -1;
                int sy = y0 < y1 ? 1 : -1;
                int err = dx - dy;

                while (true) {
                    drawPixel(x0, y0, col);
                    
                    if (x0 == x1 && y0 == y1) break;
                    
                    int e2 = 2 * err;
                    if (e2 > -dy) {
                        err -= dy;
                        x0 += sx;
                    }
                    if (e2 < dx) {
                        err += dx;
                        y0 += sy;
                    }
                }
            }

            void drawRect(int x, int y, int w, int h, Color col) {
                drawLine(x, y, x + w - 1, y, col);
                drawLine(x, y, x, y + h - 1, col);
                drawLine(x + w - 1, y, x + w - 1, y + h - 1, col);
                drawLine(x, y + h - 1, x + w - 1, y + h - 1, col);
            }

            void fillRect(int x, int y, int w, int h, Color col) {
                if (!isInitialized) return;
                
                int x1 = x < 0 ? 0 : x;
                int y1 = y < 0 ? 0 : y;
                int x2 = (x + w) > width ? width : (x + w);
                int y2 = (y + h) > height ? height : (y + h);
                
                for (int py = y1; py < y2; py++) {
                    for (int px = x1; px < x2; px++) {
                        drawPixel(px, py, col);
                    }
                }
            }

            void drawCircle(int centerX, int centerY, int radius, Color col) {
                int x = 0;
                int y = radius;
                int d = 3 - 2 * radius;
                
                while (y >= x) {
                    drawPixel(centerX + x, centerY + y, col);
                    drawPixel(centerX - x, centerY + y, col);
                    drawPixel(centerX + x, centerY - y, col);
                    drawPixel(centerX - x, centerY - y, col);
                    drawPixel(centerX + y, centerY + x, col);
                    drawPixel(centerX - y, centerY + x, col);
                    drawPixel(centerX + y, centerY - x, col);
                    drawPixel(centerX - y, centerY - x, col);
                    
                    x++;
                    if (d > 0) {
                        y--;
                        d = d + 4 * (x - y) + 10;
                    } else {
                        d = d + 4 * x + 6;
                    }
                }
            }

            void fillCircle(int centerX, int centerY, int radius, Color col) {
                for (int y = -radius; y <= radius; y++) {
                    for (int x = -radius; x <= radius; x++) {
                        if (x * x + y * y <= radius * radius) {
                            drawPixel(centerX + x, centerY + y, col);
                        }
                    }
                }
            }

            void drawChar(int x, int y, char c, Color col) {                
                if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7F) return;

                const uint8_t* glyph = font_8x16[c - 0x20];
                
                for (int yp = 0; yp < 16; yp++) {
                    unsigned char data = glyph[yp];
                    for (int xp = 0; xp < 8; xp++) {
                        if (data & (0x80 >> xp)) {
                            uint64_t locX = xp + x;
                            uint64_t locY = yp + y;

                            if (locX < width && locY < height) {
                                drawPixel(locX, locY, col);
                            }
                        } else {

                        }
                    }
                }
            }

            void drawText(int x, int y, const char* text, Color col) {
                if (!text) return;
                
                int currentX = x;
                int currentY = y;
                
                while (*text) {
                    if (*text == '\n') {
                        currentY += 8;
                        currentX = x;
                    } else {
                        drawChar(currentX, currentY, *text, col);
                        currentX += 8;
                    }
                    text++;
                }
            }

            void swapBuffers() {
                if (!isInitialized || !frontbuf) return;
                std::memcpy(frontbuf, backbuffer, width * height * sizeof(uint32_t));
            }

            int getWidth() const { return width; }
            int getHeight() const { return height; }
            int getPitch() const { return pitch; }
            
            void drawPixel(uint64_t x, uint64_t y, Color col) {
                if (!isInitialized) return;
                if (x >= width || y >= height) return;
                uint32_t pixelv = (static_cast<uint32_t>(col.r) << 16) |
                (static_cast<uint32_t>(col.g) << 8) |
                (static_cast<uint32_t>(col.b));
                backbuffer[y * width + x] = pixelv;
            }
            void clear(Color col) {
                if (!isInitialized) return;
                memset32(backbuffer, col.toRGB(), height * width);
            }
        private:
            GDI() : frontbuf(nullptr), width(0), height(0), pitch(0), bpp(0), 
                                  isInitialized(false) {}
            void* frontbuf;
            int width, height;
            int pitch;
            int bpp;
            bool isInitialized;
            uint8_t redMaskShift;
            uint8_t greenMaskShift;
            uint8_t blueMaskShift;
            uint8_t redMaskSize;
            uint8_t greenMaskSize;
            uint8_t blueMaskSize;
            
            static uint32_t backbuffer[MAX_BACKBUFFER_SIZE];
    };
    
    uint32_t GDI::backbuffer[GDI::MAX_BACKBUFFER_SIZE];
}