#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// 8 bytes Each
struct IDTEntry {
    uint16_t offset_low;    // Lower 16 bits of handler address
    uint16_t selector;      // Kernel code segment selector
    uint8_t  zero;          
    uint8_t  type_attr;     
    uint16_t offset_high;   // Upper 16 bits of handler address
} __attribute__((packed));

// IDT descriptor (like GDT descriptor)
struct IDTDescriptor {
    uint16_t limit;         // Size of IDT - 1
    uint32_t base;          // Address of IDT
} __attribute__((packed));

// 256 possible interrupts
#define IDT_ENTRIES 256

// Type attributes
#define IDT_INTERRUPT_GATE 0x8E  // Present, Ring 0, 32-bit interrupt gate
#define IDT_TRAP_GATE      0x8F  // Present, Ring 0, 32-bit trap gate

// Declarations
void idt_init();
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t type_attr);

#endif