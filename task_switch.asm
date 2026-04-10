[bits 32]

global switch_context

; void switch_context(uint32_t* old_esp, uint32_t new_esp)
; Saves callee-saved registers onto the current stack, stores ESP into *old_esp,
; loads new_esp, restores the new task's callee-saved registers, and returns
; into the new task.
switch_context:
    mov eax, [esp + 4]      ; eax = old_esp pointer
    mov ecx, [esp + 8]      ; ecx = new_esp value

    push ebp
    push ebx
    push esi
    push edi

    mov [eax], esp          ; save current ESP into *old_esp

    mov esp, ecx            ; switch to new task's stack

    pop edi
    pop esi
    pop ebx
    pop ebp

    ret                     ; "returns" into the new task
