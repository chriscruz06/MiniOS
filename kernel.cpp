#include "idt.h"
#include "keyboard.h"
#include "timer.h"
#include <stdint.h>
#include "sleep.h"

extern "C" void main() {
    idt_init();
    
    keyboard_init();
    timer_init(100);  // 100 Hz = tick per 10ms
    
    // Display my ghetto boot message
    uint16_t* vga = (uint16_t*)0xb8000;
    const char* msg = " <MiniOS> ";
    for (int i = 0; msg[i]; i++) {
        vga[i] = (0x0A << 8) | msg[i];
    }
    
    // Interrupts
    __asm__ volatile("sti");

    vga[81] = (0x0A << 8) | '!';  // Green ! when done

    // Halt loop (god willing)
    while (1) {
        __asm__ volatile("hlt");
    }
}