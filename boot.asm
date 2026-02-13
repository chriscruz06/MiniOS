[org 0x7c00]                        
KERNEL_LOCATION equ 0x10000

; ============================================================
; E820 memory map stored at 0x8000:
;   [0x8000]       = uint32_t entry_count
;   [0x8004+]      = array of E820 entries (24 bytes each)
; ============================================================
E820_MAP     equ 0x8000
E820_ENTRIES equ 0x8004

mov [BOOT_DISK], dl                 

; Set up segments and stack
xor ax, ax                          
mov ds, ax
mov bp, 0x9000       ; Stack well above everything
mov sp, bp

; ============================================================
; Step 1: Load kernel from disk FIRST (before E820)
; Load to physical address 0x10000 using ES:BX = 0x1000:0x0000
; This avoids overwriting the bootloader at 0x7C00
; Floppy geometry: 18 sectors/track, 2 heads
; ============================================================
load_kernel:
    mov ax, 0x1000          ; ES = 0x1000
    mov es, ax              ; ES:BX = 0x1000:0x0000 = physical 0x10000
    mov bx, 0x0000          ; destination offset
    mov cl, 2               ; starting sector (1-indexed, sector 2)
    mov ch, 0               ; cylinder 0
    mov dh, 0               ; head 0
    mov si, 80              ; total sectors to read

.read_loop:
    cmp si, 0
    je .read_done

    mov ah, 0x02            ; BIOS read sectors
    mov al, 1               ; read 1 sector at a time
    mov dl, [BOOT_DISK]
    int 0x13
    jc .read_loop           ; retry on error

    add bx, 512             ; advance buffer by one sector
    dec si

    ; Advance CHS to next sector
    inc cl                  ; next sector number
    cmp cl, 19              ; past sector 18? (floppy = 18 sectors/track)
    jne .read_loop

    mov cl, 1               ; reset to sector 1
    inc dh                  ; next head
    cmp dh, 2               ; past head 1?
    jne .read_loop

    mov dh, 0               ; reset head
    inc ch                  ; next cylinder
    jmp .read_loop

.read_done:

; ============================================================
; Step 2: Set text mode
; ============================================================                                    
    mov ah, 0x0
    mov al, 0x3
    int 0x10                ; text mode

; ============================================================
; Step 3: Detect memory map using BIOS INT 0x15, EAX=0xE820
; Need to reset ES back to 0 for E820 (it was 0x1000 for disk load)
; ============================================================
detect_memory:
    xor ax, ax
    mov es, ax              ; ES = 0 (ES:DI points to E820_ENTRIES)
    mov di, E820_ENTRIES    ; DI = destination for entries
    xor ebx, ebx           ; EBX = 0 to start
    xor bp, bp              ; BP = entry counter
    mov edx, 0x534D4150    ; 'SMAP' magic number

.e820_loop:
    mov eax, 0xE820        ; Function number
    mov ecx, 24            ; Ask for 24 bytes per entry
    int 0x15

    jc .e820_done           ; Carry flag = error or done
    cmp eax, 0x534D4150    ; Should return 'SMAP'
    jne .e820_done

    ; Check if entry length is 0 (some BIOSes return empty entries)
    cmp dword [es:di + 8], 0
    jne .valid_entry
    cmp dword [es:di + 12], 0
    je .skip_entry          ; Length is 0, skip this entry

.valid_entry:
    inc bp                  ; Count valid entries
    add di, 24             ; Move to next slot

.skip_entry:
    test ebx, ebx
    jz .e820_done           ; EBX = 0 means last entry
    jmp .e820_loop

.e820_done:
    mov [E820_MAP], bp      ; Store entry count

; ============================================================
; Step 4: Enter protected mode
; ============================================================
CODE_SEG equ GDT_code - GDT_start
DATA_SEG equ GDT_data - GDT_start

    cli
    lgdt [GDT_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:start_protected_mode

    jmp $
                                    
BOOT_DISK: db 0

GDT_start:
    GDT_null:
        dd 0x0
        dd 0x0

    GDT_code:
        dw 0xffff
        dw 0x0
        db 0x0
        db 0b10011010
        db 0b11001111
        db 0x0

    GDT_data:
        dw 0xffff
        dw 0x0
        db 0x0
        db 0b10010010
        db 0b11001111
        db 0x0

GDT_end:

GDT_descriptor:
    dw GDT_end - GDT_start - 1
    dd GDT_start


[bits 32]
start_protected_mode:
    mov ax, DATA_SEG
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	mov ebp, 0x90000		
	mov esp, ebp

    jmp KERNEL_LOCATION

                                     
 
times 510-($-$$) db 0              
dw 0xaa55