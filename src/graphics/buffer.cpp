#include "buffer.hpp"
#include <string.h>

Buffer::Buffer(limine_framebuffer* fb) {
    address = reinterpret_cast<uint32_t*>(fb->address);
    width = fb->width;
    height = fb->height;
    pitch = fb->pitch / 4;
    red_mask_size = fb->red_mask_size;
    red_mask_shift = fb->red_mask_shift;
    green_mask_size = fb->green_mask_size;
    green_mask_shift = fb->green_mask_shift;
    blue_mask_size = fb->blue_mask_size;
    blue_mask_shift = fb->blue_mask_shift;
}

Buffer::~Buffer() {
    
}

void Buffer::putPixel(uint64_t x, uint64_t y, Color color) {
    if (x >= width || y >= height) return;
    address[y * pitch + x] = color;
}
extern "C" void* memset32(void* dest, uint32_t value, uint64_t count);
void Buffer::clear(Color color) {
    memset32(address, color, height * width);
}

uint64_t Buffer::getWidth() {
    return width;
}

uint64_t Buffer::getHeight() {
    return height;
}

void* Buffer::getRaw() {
    return (void*)address;
}

uint64_t Buffer::getPitch(){
    return pitch;
}

Color Buffer::getPixel(uint64_t x, uint64_t y) {
    if (x >= width || y >= height) return Color{0, 0, 0};
    uint64_t pixelColor = address[y * pitch + x];
    Color color;
    color.r = (pixelColor >> 16) & 0xFF;
    color.g = (pixelColor >> 8) & 0xFF;
    color.b = pixelColor & 0xFF;
    return color;
}