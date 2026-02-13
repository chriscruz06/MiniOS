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

// Read a 16-bit word from a port (needed for ATA PIO data transfers)
static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Write a 16-bit word to a port
static inline void outw(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %0, %1" : : "a"(data), "Nd"(port));
}

// Small delay for safety (reading port 0x80 takes ~1µs on ISA bus)
static inline void io_wait() {
    outb(0x80, 0);
}

#endif