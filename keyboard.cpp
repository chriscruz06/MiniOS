#include "keyboard.h"
#include "isr.h"
#include "ports.h"

static uint16_t* vga = (uint16_t*)0xb8000;
static int vga_index = 0;

static const char scancode_to_ascii[] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

static void print_char(char c) {
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

static void keyboard_callback(registers_t* regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);
    
    if (!(scancode & 0x80)) {
        if (scancode < sizeof(scancode_to_ascii)) {
            char c = scancode_to_ascii[scancode];
            if (c != 0) {
                print_char(c);
            }
        }
    }
}

void keyboard_init() {
    register_interrupt_handler(33, keyboard_callback);  // IRQ1 = interrupt 33
}