#include "framebuffer.hpp"
#include <x86_64/requests.hpp>
#include <new>

// once i set up memory, this will be on the heap
alignas(Buffer) static uint8_t stackBuffer[sizeof(Buffer)];
static bool bufferExists = false;

Framebuffer::Framebuffer() {
    if(!framebuffer_request.response){
        // panic();
        while (1);;
    }

    auto count = framebuffer_request.response->framebuffer_count;
    
    if(count == 0){
        // panic();
        while (1);;
    }

    for(auto i = 0; i < count; i++){
        auto fb = framebuffer_request.response->framebuffers[i];
        if(fb->memory_model == LIMINE_FRAMEBUFFER_RGB){
            buffer = new (stackBuffer) Buffer(fb);
            break;
        }
    }

}

Framebuffer::~Framebuffer() {
}

void Framebuffer::putPixel(uint64_t x, uint64_t y, Color color) {
    buffer->putPixel(x, y, color);
}

void Framebuffer::clear(Color color) {
    buffer->clear(color);
}

uint64_t Framebuffer::getWidth() {
    return buffer->getWidth();
}

uint64_t Framebuffer::getHeight() {
    return buffer->getHeight();
}

Color Framebuffer::getPixel(uint64_t x, uint64_t y) {
    return buffer->getPixel(x, y);
}

void* Framebuffer::getRaw(){
    return buffer->getRaw();
}

uint64_t Framebuffer::getPitch(){
    return buffer->getPitch();
}