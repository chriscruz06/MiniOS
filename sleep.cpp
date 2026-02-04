#include "sleep.h"
#include "timer.h"

void sleep_ticks(uint32_t ticks) {
    uint32_t start = timer_get_ticks();
    while (timer_get_ticks() - start < ticks) {
        __asm__ volatile("hlt"); 
    }
}

void sleep_ms(uint32_t milliseconds) {
    // At 100Hz, 1 tick = 10ms, so ms/10 = ticks needed
    // Add 1 to round up (sleep at LEAST this long)
    uint32_t ticks = (milliseconds + 9) / 10;
    sleep_ticks(ticks);
}