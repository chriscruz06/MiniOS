#include "timer.h"
#include "isr.h"
#include "ports.h"

static volatile uint32_t ticks = 0;

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

// PIT base frequency (1.193182 MHz)
#define PIT_BASE_FREQ 1193180

static void timer_callback(registers_t* regs) {
    (void)regs;
    ticks++;

    // REMOVE AFTER TESTING CHRIS
    uint16_t* vga = (uint16_t*)0xb8000;
    vga[79] = (0x0E << 8) | ('0' + (ticks % 10));
}

void timer_init(uint32_t frequency) {
    // Register our callback for IRQ0 (interrupt 32)
    register_interrupt_handler(32, timer_callback);
    
    uint32_t divisor = PIT_BASE_FREQ / frequency;
    
    // Send command byte: channel 0, lobyte/hibyte, square wave mode
    outb(PIT_COMMAND, 0x36);
    
    // Send divisor (low first, then high)
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_ticks() {
    return ticks;
}