#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

// Page size = 4KB
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

// Page directory/table entry flags
#define PTE_PRESENT    0x001   // Page is present in memory
#define PTE_WRITABLE   0x002   // Page is writable
#define PTE_USER       0x004   // Page accessible from user mode
#define PTE_WRITETHROUGH 0x008 // Write-through caching
#define PTE_NOCACHE    0x010   // Disable caching
#define PTE_ACCESSED   0x020   // CPU has read this page
#define PTE_DIRTY      0x040   // Page has been written to (PTE only)
#define PTE_4MB        0x080   // 4MB page (PDE only)

// Helpers
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~0xFFF)
#define PAGE_ALIGN_UP(addr)   (((addr) + 0xFFF) & ~0xFFF)

// Extract indices from a virtual address
#define PDE_INDEX(vaddr) (((vaddr) >> 22) & 0x3FF)  // Top 10 bits
#define PTE_INDEX(vaddr) (((vaddr) >> 12) & 0x3FF)  // Middle 10 bits

void paging_init();
void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);

#endif