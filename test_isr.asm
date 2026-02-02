[bits 32]
section .text

global test_isr_handler

; Minimal ISR that just returns
test_isr_handler:
    ; Send EOI to PIC (required for hardware interrupts)
    push eax
    mov al, 0x20
    out 0x20, al
    pop eax
    iret
