#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>

// Initialize  kernel heap
void kheap_init();

// Allocate size bytes from  kernel heap
void* kmalloc(uint32_t size);

// Free a previously allocated pointer
void kfree(void* ptr);

// Stats
uint32_t kheap_get_total_bytes();
uint32_t kheap_get_used_bytes();
uint32_t kheap_get_free_bytes();
uint32_t kheap_get_block_count();

// Debug: dump heap state
void kheap_dump();

#endif