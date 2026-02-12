#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// E820 Memory Map structures (passed from bootloader)
// ============================================================

// E820 memory region types
#define E820_USABLE           1
#define E820_RESERVED         2
#define E820_ACPI_RECLAIMABLE 3
#define E820_ACPI_NVS         4
#define E820_BAD_MEMORY       5

// Where the bootloader stores the E820 map
#define E820_COUNT_ADDR  0x8000
#define E820_ENTRIES_ADDR 0x8004

struct E820Entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attrs;
} __attribute__((packed));

// ============================================================
// PMM Interface
// ============================================================

#define PAGE_SIZE 4096

// Initialize the PMM - reads E820 map, sets up bitmap
void pmm_init();

// Allocate a single 4KB page frame, returns physical address (or nullptr if OOM)
void* pmm_alloc_frame();

// Free a previously allocated page frame
void pmm_free_frame(void* frame);

// Check if a specific frame is allocated
bool pmm_is_frame_allocated(void* frame);

// Stats
uint32_t pmm_get_total_frames();
uint32_t pmm_get_used_frames();
uint32_t pmm_get_free_frames();
uint32_t pmm_get_total_memory_kb();

// Debug: dump the E820 map and PMM stats to screen
void pmm_dump();

#endif