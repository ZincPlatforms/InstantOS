#include "buffer.hpp"

Buffer::Buffer(limine_framebuffer* fb) {
    address = reinterpret_cast<uint32_t*>(fb->address);
    width = fb->width;
    height = fb->height;
    pitch = fb->pitch / 4;
}

Buffer::~Buffer() {
    
}

void Buffer::putPixel(uint64_t x, uint64_t y, Color color) {
    if (x >= width || y >= height) return;
    uint64_t pixelColor = (static_cast<uint64_t>(color.r) << 16) |
                           (static_cast<uint64_t>(color.g) << 8) |
                           (static_cast<uint64_t>(color.b));
    address[y * pitch + x] = pixelColor;
}

void Buffer::clear(Color color) {
    for (uint64_t y = 0; y < height; y++) {
        for (uint64_t x = 0; x < width; x++) {
            putPixel(x, y, color);
        }
    }
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