#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "isr.h"

#define TASK_STACK_SIZE 4096
#define MAX_TASKS 16

enum TaskState {
    TASK_READY    = 0,
    TASK_RUNNING  = 1,
    TASK_SLEEPING = 2,
    TASK_DEAD     = 3
};

struct Task {
    uint32_t    id;
    uint32_t    esp;          // Saved stack pointer
    uint32_t    stack_base;   // kmalloc'd base for freeing; 0 for task 0 (boot stack)
    TaskState   state;
    uint32_t    sleep_until;  // Tick count to wake at
    const char* name;
};

void   task_init();
int    task_create(void (*entry)(), const char* name);
void   task_exit();
void   task_yield();
void   task_sleep(uint32_t ticks);
void   task_schedule(registers_t* regs);
int    task_get_current_id();
int    task_get_count();
Task*  task_get_list();

extern "C" void switch_context(uint32_t* old_esp, uint32_t new_esp);

#endif
