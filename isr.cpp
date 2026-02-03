#include "isr.h"
#include "ports.h"

// Array of handler function pointers (one per interrupt 0-47)
static isr_handler_t interrupt_handlers[48] = {0};

// Register a handler for interrupt n
void register_interrupt_handler(uint8_t n, isr_handler_t handler) {
    if (n < 48) {
        interrupt_handlers[n] = handler;
    }
}

// Main ISR dispatcher - called from assembly
extern "C" void isr_handler(registers_t* regs) {
    uint8_t int_no = regs->int_no;
    
    // Call registered handler if one exists
    if (int_no < 48 && interrupt_handlers[int_no] != 0) {
        interrupt_handlers[int_no](regs);
    }
    
    // Send EOI for hardware interrupts (IRQs 0-15 = interrupts 32-47)
    if (int_no >= 32 && int_no < 48) {
        if (int_no >= 40) {
            outb(0xA0, 0x20);  // EOI to slave PIC
        }
        outb(0x20, 0x20);      // EOI to master PIC
    }
}