#pragma once

#include <cstdint>
#include <array>
#include <cpu/idt/interrupt.hpp>
#include <x86_64/ports.hpp>

class Keyboard : public Interrupt {
public:
    void initialize() override {
        scancodeToAscii = {
            0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
            '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
            0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
            0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
            '*', 0, ' '
        };

        scancodeToAsciiShift = {
            0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
            '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
            0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
            0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
            '*', 0, ' '
        };

        shiftPressed = false;
        ctrlPressed = false;
        altPressed = false;
        capsLock = false;
        
        uint8_t status;
        
        outb(0x64, 0xAD);
        outb(0x64, 0xA7);
        
        while (true) {
            status = inb(0x64);
            if (!(status & 1)) break;
            inb(0x60);
        }

        outb(0x64, 0x20);
        do {
            status = inb(0x64);
        } while (!(status & 1));
        uint8_t config = inb(0x60);

        config |= 0x01;
        config &= ~0x10;

        outb(0x64, 0x60);
        do {
            status = inb(0x64);
        } while (status & 2);
        outb(0x60, config);

        outb(0x64, 0xAE);

        do {
            status = inb(0x64);
        } while (status & 2);
        outb(0x60, 0xF4);

        uint8_t response = 0;
        int timeout = 100000;
        while (timeout-- > 0) {
            status = inb(0x64);
            if (status & 1) {
                response = inb(0x60);
                break;
            }
        }
    }

    void Run(InterruptFrame* frame) override {
        uint8_t scancode = inb(0x60);
        
        if (scancode & 0x80) {
            scancode &= 0x7F;
            if (scancode == 0x2A || scancode == 0x36) {
                shiftPressed = false;
            } else if (scancode == 0x1D) {
                ctrlPressed = false;
            } else if (scancode == 0x38) {
                altPressed = false;
            }
        } else {
            if (scancode == 0x2A || scancode == 0x36) {
                shiftPressed = true;
            } else if (scancode == 0x1D) {
                ctrlPressed = true;
            } else if (scancode == 0x38) {
                altPressed = true;
            } else if (scancode == 0x3A) {
                capsLock = !capsLock;
            } else {
                if (scancode < 58) {
                    char c = 0;
                    bool useShift = shiftPressed ^ capsLock;
                    
                    if (useShift) {
                        c = scancodeToAsciiShift[scancode];
                    } else {
                        c = scancodeToAscii[scancode];
                    }
                    
                    if (c != 0) {
                        int nextHead = (bufferHead + 1) % BUFFER_SIZE;
                        if (nextHead != bufferTail) {
                            buffer[bufferHead] = c;
                            bufferHead = nextHead;
                        }
                    }
                }
            }
        }

        sendEOI();
    }
    
    bool hasKey() { return bufferHead != bufferTail; }
    
    char getKey() {
        if (!hasKey()) return 0;
    
        char c = buffer[bufferTail];
        bufferTail = (bufferTail + 1) % BUFFER_SIZE;
        return c;
    }
    
private:
    std::array<char, 58> scancodeToAscii;
    std::array<char, 58> scancodeToAsciiShift;
    
    static const int BUFFER_SIZE = 256;
    char buffer[BUFFER_SIZE];
    int bufferHead = 0;
    int bufferTail = 0;
    
    bool shiftPressed;
    bool ctrlPressed;
    bool altPressed;
    bool capsLock;
};