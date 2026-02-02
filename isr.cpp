#include "isr.h"
#include "ports.h"

static uint16_t* vga = (uint16_t*)0xb8000;
static int vga_index = 0;

// Scancode to ASCII lookup table (US keyboard, lowercase only)
static const char scancode_to_ascii[] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

void print_char(char c) {
    if (c == '\n') {
        vga_index = (vga_index / 80 + 1) * 80;
    } else if (c == '\b') {
        if (vga_index > 0) {
            vga_index--;
            vga[vga_index] = (0x0F << 8) | ' ';
        }
    } else {
        vga[vga_index++] = (0x0F << 8) | c;
    }
    
    // Scroll if needed
    if (vga_index >= 80 * 25) {
        for (int i = 0; i < 80 * 24; i++) {
            vga[i] = vga[i + 80];
        }
        for (int i = 80 * 24; i < 80 * 25; i++) {
            vga[i] = (0x0F << 8) | ' ';
        }
        vga_index = 80 * 24;
    }
}

extern "C" void isr_handler(registers_t* regs) {
    if (regs->int_no >= 32 && regs->int_no < 48) {
        uint8_t irq = regs->int_no - 32;
        
        // IRQ1 = Keyboard
        if (irq == 1) {
            uint8_t scancode = inb(0x60);
            
            // Only handle key press (not release - those have bit 7 set)
            if (!(scancode & 0x80)) {
                if (scancode < sizeof(scancode_to_ascii)) {
                    char c = scancode_to_ascii[scancode];
                    if (c != 0) {
                        print_char(c);
                    }
                }
            }
        }
        
        // Send EOI
        if (irq >= 8) {
            outb(0xA0, 0x20);
        }
        outb(0x20, 0x20);
    }
}