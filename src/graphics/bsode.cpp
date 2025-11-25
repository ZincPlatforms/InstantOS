#include "framebuffer.hpp"
#include "bsod.hpp"
extern Framebuffer* fb;

void _bsod(){
    fb->clear(0x0079d8);
    for(int x = 0; x < BSOD_WIDTH; x++){
        for(int y = 0; y < BSOD_HEIGHT; y++){
            auto pixel = bsod[y * BSOD_WIDTH + x];
            if(pixel == 0x0) continue;
            fb->putPixel(x, y, pixel);
        }
    }
}