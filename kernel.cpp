#include "idt.h"

extern "C" void main() {
    idt_init();
    
    __asm__ volatile("sti");
    
    uint16_t* vga = (uint16_t*)0xb8000;
    const char* msg = "MiniOS> ";
    for (int i = 0; msg[i]; i++) {
        vga[i] = (0x0A << 8) | msg[i];
    }
    
    while (1) {
        __asm__ volatile("hlt");
    }
}