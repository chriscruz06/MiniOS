#ifndef ISR_H
#define ISR_H

#include <stdint.h>

// Structure matching what we pushed in isr_common
struct registers_t {
    uint32_t gs, fs, es, ds;                         // Segment registers
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // pusha
    uint32_t int_no, err_code;                       // Interrupt number and error code
    uint32_t eip, cs, eflags, useresp, ss;           // Pushed by CPU
} __attribute__((packed));

// ISR declarations (defined in isr.asm)
extern "C" {
    void isr0();  void isr1();  void isr2();  void isr3();
    void isr4();  void isr5();  void isr6();  void isr7();
    void isr8();  void isr9();  void isr10(); void isr11();
    void isr12(); void isr13(); void isr14(); void isr15();
    void isr16(); void isr17(); void isr18(); void isr19();
    void isr20(); void isr21(); void isr22(); void isr23();
    void isr24(); void isr25(); void isr26(); void isr27();
    void isr28(); void isr29(); void isr30(); void isr31();
    void isr32(); void isr33(); void isr34(); void isr35();
    void isr36(); void isr37(); void isr38(); void isr39();
    void isr40(); void isr41(); void isr42(); void isr43();
    void isr44(); void isr45(); void isr46(); void isr47();
}

// Function pointer type for interrupt handlers
typedef void (*isr_handler_t)(registers_t*);

// Register a handler for a specific interrupt number
void register_interrupt_handler(uint8_t n, isr_handler_t handler);

#endif // warhammer darktide is so fun they need to add adeptus mechanicus as a class tho