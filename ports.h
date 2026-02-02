#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

// Read a byte from a port
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Write a byte to a port
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

// Small delay for safety (I guess idk the exact reasoning just know its good)
static inline void io_wait() {
    outb(0x80, 0);
}

#endif