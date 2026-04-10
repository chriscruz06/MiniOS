#include "task.h"
#include "kheap.h"
#include "timer.h"

static Task tasks[MAX_TASKS];
static int current_task = 0;
static int next_id = 0;
static bool scheduler_enabled = false;

void task_init() {
    // Mark all slots dead (TASK_DEAD=3, not 0, so must set explicitly)
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state      = TASK_DEAD;
        tasks[i].stack_base = 0;
        tasks[i].name       = 0;
        tasks[i].esp        = 0;
        tasks[i].sleep_until = 0;
    }

    // Bootstrap the currently-running kernel as task 0 (the shell)
    tasks[0].id         = 0;
    tasks[0].state      = TASK_RUNNING;
    tasks[0].name       = "shell";
    tasks[0].esp        = 0;   // filled on first switch away
    tasks[0].stack_base = 0;   // uses boot stack at 0x90000, not kmalloc'd

    current_task      = 0;
    next_id           = 1;
    scheduler_enabled = true;
}

int task_create(void (*entry)(), const char* name) {
    // Find a free slot
    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;

    // Allocate a kernel stack
    uint8_t* stack = (uint8_t*)kmalloc(TASK_STACK_SIZE);
    if (!stack) return -1;

    // Build the initial switch_context frame on the new stack.
    // Stack grows downward from stack + TASK_STACK_SIZE.
    // Layout when switch_context does its pops then ret:
    //   pop edi, pop esi, pop ebx, pop ebp  <- four zeros
    //   ret -> pops entry as the return address
    //   (if entry returns, it lands on task_exit)
    uint32_t* sp = (uint32_t*)(stack + TASK_STACK_SIZE);
    *(--sp) = (uint32_t)task_exit;  // safety net if entry() returns
    *(--sp) = (uint32_t)entry;      // switch_context's ret pops this
    *(--sp) = 0;                    // ebp
    *(--sp) = 0;                    // ebx
    *(--sp) = 0;                    // esi
    *(--sp) = 0;                    // edi  <- ESP points here

    tasks[slot].id          = next_id++;
    tasks[slot].esp         = (uint32_t)sp;
    tasks[slot].stack_base  = (uint32_t)stack;
    tasks[slot].state       = TASK_READY;
    tasks[slot].sleep_until = 0;
    tasks[slot].name        = name;

    return tasks[slot].id;
}

void task_schedule(registers_t* regs) {
    (void)regs;
    if (!scheduler_enabled) return;

    // Wake sleeping tasks
    uint32_t now = timer_get_ticks();
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && tasks[i].sleep_until <= now) {
            tasks[i].state = TASK_READY;
        }
    }

    // Reap dead tasks (free stacks while we are NOT on them)
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD && tasks[i].stack_base != 0) {
            kfree((void*)tasks[i].stack_base);
            tasks[i].stack_base = 0;
        }
    }

    // Round-robin: find next READY task
    int next = -1;
    for (int i = 1; i <= MAX_TASKS; i++) {
        int idx = (current_task + i) % MAX_TASKS;
        if (tasks[idx].state == TASK_READY) {
            next = idx;
            break;
        }
    }
    if (next == -1) return;  // nothing else to run

    // Transition states
    int old = current_task;
    if (tasks[old].state == TASK_RUNNING) tasks[old].state = TASK_READY;
    current_task = next;
    tasks[current_task].state = TASK_RUNNING;

    switch_context(&tasks[old].esp, tasks[current_task].esp);
}

void task_exit() {
    if (current_task == 0) return;  // never kill the shell
    tasks[current_task].state = TASK_DEAD;
    task_yield();
    // Safety net — should never reach here
    while (1) { __asm__ volatile("hlt"); }
}

void task_yield() {
    __asm__ volatile("cli");
    task_schedule(0);
    __asm__ volatile("sti");
}

void task_sleep(uint32_t ticks) {
    tasks[current_task].sleep_until = timer_get_ticks() + ticks;
    tasks[current_task].state       = TASK_SLEEPING;
    task_yield();
}

int task_get_current_id() {
    return tasks[current_task].id;
}

int task_get_count() {
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_DEAD) count++;
    }
    return count;
}

Task* task_get_list() {
    return tasks;
}
