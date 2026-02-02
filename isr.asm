[bits 32]

; External C handler
extern isr_handler

; Macro for ISRs that dont push an error code
%macro ISR_NO_ERR 1
global isr%1
isr%1:
    push dword 0        ; Push dummy error code
    push dword %1       ; Push interrupt number
    jmp isr_common
%endmacro

; Macro for ISRs that DO push an error code
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1       ; Push interrupt number (error code already on stack)
    jmp isr_common
%endmacro

; CPU Exceptions (0-31) (Feels like those bible geneologies)
ISR_NO_ERR 0   ; Division by zero
ISR_NO_ERR 1   ; Debug
ISR_NO_ERR 2   ; NMI
ISR_NO_ERR 3   ; Breakpoint
ISR_NO_ERR 4   ; Overflow
ISR_NO_ERR 5   ; Bound range exceeded
ISR_NO_ERR 6   ; Invalid opcode
ISR_NO_ERR 7   ; Device not available
ISR_ERR    8   ; Double fault
ISR_NO_ERR 9   ; Coprocessor segment overrun (legacy, dont know if its necessary but whatevs)
ISR_ERR    10  ; Invalid TSS
ISR_ERR    11  ; Segment not present
ISR_ERR    12  ; Stack segment fault
ISR_ERR    13  ; General protection fault
ISR_ERR    14  ; Page fault
ISR_NO_ERR 15  ; Reserved
ISR_NO_ERR 16  ; x87 FPU error
ISR_ERR    17  ; Alignment check
ISR_NO_ERR 18  ; Machine check
ISR_NO_ERR 19  ; SIMD floating point
ISR_NO_ERR 20  ; Virtualization
ISR_NO_ERR 21  ; Reserved
ISR_NO_ERR 22  ; Reserved
ISR_NO_ERR 23  ; Reserved
ISR_NO_ERR 24  ; Reserved
ISR_NO_ERR 25  ; Reserved
ISR_NO_ERR 26  ; Reserved
ISR_NO_ERR 27  ; Reserved
ISR_NO_ERR 28  ; Reserved
ISR_NO_ERR 29  ; Reserved
ISR_ERR    30  ; Security exception
ISR_NO_ERR 31  ; Reserved

; IRQs (32-47) - Hardware interrupts remapped from PIC
ISR_NO_ERR 32  ; IRQ0  - Timer
ISR_NO_ERR 33  ; IRQ1  - Keyboard
ISR_NO_ERR 34  ; IRQ2  - Cascade
ISR_NO_ERR 35  ; IRQ3  - COM2
ISR_NO_ERR 36  ; IRQ4  - COM1
ISR_NO_ERR 37  ; IRQ5  - LPT2
ISR_NO_ERR 38  ; IRQ6  - Floppy (ayo)
ISR_NO_ERR 39  ; IRQ7  - LPT1
ISR_NO_ERR 40  ; IRQ8  - CMOS RTC
ISR_NO_ERR 41  ; IRQ9  - Free
ISR_NO_ERR 42  ; IRQ10 - Free
ISR_NO_ERR 43  ; IRQ11 - Free
ISR_NO_ERR 44  ; IRQ12 - PS/2 Mouse
ISR_NO_ERR 45  ; IRQ13 - FPU
ISR_NO_ERR 46  ; IRQ14 - Primary ATA
ISR_NO_ERR 47  ; IRQ15 - Secondary ATA

; Common handler - saves all registers, calls C, restores
isr_common:
    ; Save all general purpose registers
    pusha
    
    ; Save segment registers
    push ds
    push es
    push fs
    push gs
    
    ; Load kernel data segment
    mov ax, 0x10        ; DATA_SEG from your GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Push pointer to stack (registers_t struct)
    push esp
    
    ; Call C handler
    call isr_handler
    
    ; Remove pushed pointer
    add esp, 4
    
    ; Restore segment registers
    pop gs
    pop fs
    pop es
    pop ds
    
    ; Restore general purpose registers
    popa ; she call me big poppa
    
    ; Remove error code and interrupt number
    add esp, 8
    
    ; Return from interrupt
    iret