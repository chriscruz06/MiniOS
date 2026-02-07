#include "idt.h"
#include "keyboard.h"
#include "timer.h"
#include "shell.h"

extern "C" void main() {
    idt_init();
    keyboard_init();
    timer_init(100);
    shell_init();
    
    __asm__ volatile("sti");
    
    while (1) {
        __asm__ volatile("hlt");
    }
}