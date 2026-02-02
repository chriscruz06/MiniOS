#include "ports.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

// Remap PIC so IRQs 0-15 become interrupts 32-47
void pic_remap() {
    // Save masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // Start initialization sequence (ICW1)
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();
    
    // Set vector offsets (ICW2)
    outb(PIC1_DATA, 0x20);  // IRQ 0-7  -> INT 32-39
    io_wait();
    outb(PIC2_DATA, 0x28);  // IRQ 8-15 -> INT 40-47
    io_wait();
    
    // Tell PICs about each other (ICW3)
    outb(PIC1_DATA, 0x04);  // IRQ2 has slave
    io_wait();
    outb(PIC2_DATA, 0x02);  // Slave ID 2
    io_wait();
    
    // Set 8086 mode (ICW4)
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();
    
    // Restore masks (or set new ones)
    // Enable only keyboard (IRQ1) for now
    outb(PIC1_DATA, 0xFD);  // 11111101 - only IRQ1 enabled -- she bi on my nary
    outb(PIC2_DATA, 0xFF);  // All disabled
}