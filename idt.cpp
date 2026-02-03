// idt.cpp
#include "idt.h"
#include "isr.h"
#include "pic.h"

#define IDT_ADDRESS 0x10000
#define IDT_DESC_ADDRESS 0x10800

#define idt ((IDTEntry*)IDT_ADDRESS)
#define idt_descriptor (*(IDTDescriptor*)IDT_DESC_ADDRESS)

extern "C" void idt_load(uint32_t idt_ptr);

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t type_attr) {
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = type_attr;
}

void idt_init() {
    __asm__ volatile("cli");

    idt_descriptor.limit = (sizeof(IDTEntry) * IDT_ENTRIES) - 1;
    idt_descriptor.base  = (uint32_t)idt;

    // Clear all entries
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // CPU Exceptions (0-31)
    idt_set_gate(0,  (uint32_t)isr0,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(10, (uint32_t)isr10, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(11, (uint32_t)isr11, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(12, (uint32_t)isr12, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(13, (uint32_t)isr13, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(14, (uint32_t)isr14, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(15, (uint32_t)isr15, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(16, (uint32_t)isr16, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(17, (uint32_t)isr17, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(18, (uint32_t)isr18, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(19, (uint32_t)isr19, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(20, (uint32_t)isr20, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(21, (uint32_t)isr21, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(22, (uint32_t)isr22, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(23, (uint32_t)isr23, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(24, (uint32_t)isr24, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(25, (uint32_t)isr25, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(26, (uint32_t)isr26, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(27, (uint32_t)isr27, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(28, (uint32_t)isr28, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(29, (uint32_t)isr29, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(30, (uint32_t)isr30, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(31, (uint32_t)isr31, 0x08, IDT_INTERRUPT_GATE);

    // IRQs (32-47)
    idt_set_gate(32, (uint32_t)isr32, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(33, (uint32_t)isr33, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(34, (uint32_t)isr34, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(35, (uint32_t)isr35, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(36, (uint32_t)isr36, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(37, (uint32_t)isr37, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(38, (uint32_t)isr38, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(39, (uint32_t)isr39, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(40, (uint32_t)isr40, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(41, (uint32_t)isr41, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(42, (uint32_t)isr42, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(43, (uint32_t)isr43, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(44, (uint32_t)isr44, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(45, (uint32_t)isr45, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(46, (uint32_t)isr46, 0x08, IDT_INTERRUPT_GATE);
    idt_set_gate(47, (uint32_t)isr47, 0x08, IDT_INTERRUPT_GATE);

    pic_remap();

    idt_load((uint32_t)&idt_descriptor);
}