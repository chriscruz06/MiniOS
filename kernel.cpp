#include "idt.h"
#include "keyboard.h"
#include "timer.h"
#include "pmm.h"
#include "shell.h"
#include <stdint.h>
#include "paging.h"
#include "kheap.h"


extern "C" void main() {
    idt_init();
    
    // Initialize drivers
    keyboard_init();
    timer_init(100);  // 100 Hz = tick per 10ms
    
    // Initialize physical memory manager (reads E820 map from bootloader)
    pmm_init();

    // Initialize paging (identity maps first 1MB, enables paging)
    paging_init();
    
    // Heap allocator 
    kheap_init();

    // Start shell (this clears screen and shows prompt)
    shell_init();

    // Interrupts
    __asm__ volatile("sti");
    
    // Halt loop (god willing)
    while (1) {
        __asm__ volatile("hlt");
    }
}