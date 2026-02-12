#include "paging.h"
#include "isr.h"
#include "pmm.h"

// Page directory - allocated from PMM during init
static uint32_t* page_directory;

// ============================================================================
// VGA helpers for panic output
// ============================================================================

static uint16_t* vga = (uint16_t*)0xb8000;

static void vga_print_at(int row, int col, const char* msg, uint8_t color) {
    int idx = row * 80 + col;
    for (int i = 0; msg[i]; i++) {
        vga[idx + i] = (color << 8) | msg[i];
    }
}

static void vga_print_hex_at(int row, int col, uint32_t val, uint8_t color) {
    const char* hex = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    vga_print_at(row, col, buf, color);
}

// ============================================================================
// Page fault handler (ISR 14)
// ============================================================================

static void page_fault_handler(registers_t* regs) {
    // CR2 holds the faulting virtual address
    uint32_t faulting_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_addr));

    uint32_t err = regs->err_code;
    // bit 0: 0 = page not present, 1 = protection violation
    // bit 1: 0 = read, 1 = write
    // bit 2: 0 = kernel mode, 1 = user mode

    // Panic with debug info
    for (int i = 80 * 10; i < 80 * 16; i++) {
        vga[i] = (0x0C << 8) | ' ';
    }

    vga_print_at(10, 0, "=== PAGE FAULT ===", 0x4F);

    vga_print_at(11, 0, "Faulting address: ", 0x0C);
    vga_print_hex_at(11, 18, faulting_addr, 0x0E);

    vga_print_at(12, 0, "Error code: ", 0x0C);
    vga_print_hex_at(12, 12, err, 0x0E);

    vga_print_at(13, 0, (err & 1) ? "Protection violation" : "Page not present", 0x0C);
    vga_print_at(14, 0, (err & 2) ? "Write access" : "Read access", 0x0C);
    vga_print_at(15, 0, (err & 4) ? "User mode" : "Kernel mode", 0x0C);

    // Halt - unrecoverable for now
    __asm__ volatile("cli; hlt");
}

// ============================================================================
// Allocate a zeroed page from PMM (for page tables)
// ============================================================================

static uint32_t* alloc_page_table() {
    uint32_t* table = (uint32_t*)pmm_alloc_frame();

    if (!table) {
        vga_print_at(10, 0, "PAGING: OUT OF MEMORY", 0x4F);
        __asm__ volatile("cli; hlt");
    }

    // Zero out the table (1024 entries)
    for (int i = 0; i < 1024; i++) {
        table[i] = 0;
    }

    return table;
}

// ============================================================================
// Map a single 4KB page: virtual_addr -> physical_addr
// ============================================================================

void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    uint32_t pde_idx = PDE_INDEX(virtual_addr);
    uint32_t pte_idx = PTE_INDEX(virtual_addr);

    // Check if a page table exists for this directory entry
    if (!(page_directory[pde_idx] & PTE_PRESENT)) {
        uint32_t* new_table = alloc_page_table();
        page_directory[pde_idx] = (uint32_t)new_table | PTE_PRESENT | PTE_WRITABLE;
    }

    // Get the page table address (mask off the flags in lower 12 bits)
    uint32_t* page_table = (uint32_t*)(page_directory[pde_idx] & 0xFFFFF000);

    // Set the page table entry
    page_table[pte_idx] = PAGE_ALIGN_DOWN(physical_addr) | (flags & 0xFFF);
}

// ============================================================================
// Identity map a range: every virtual address maps to the same physical address
// ============================================================================

static void identity_map_range(uint32_t start, uint32_t end, uint32_t flags) {
    start = PAGE_ALIGN_DOWN(start);
    end = PAGE_ALIGN_UP(end);

    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        map_page(addr, addr, flags);
    }
}

// ============================================================================
// Initialize paging
// ============================================================================

void paging_init() {
    // Allocate the page directory from PMM
    page_directory = (uint32_t*)pmm_alloc_frame();
    if (!page_directory) {
        vga_print_at(10, 0, "PAGING: CANNOT ALLOC PAGE DIR", 0x4F);
        __asm__ volatile("cli; hlt");
    }

    // Clear the page directory (1024 entries)
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0;
    }

    // Identity map the first 1MB
    // Covers: IVT, BIOS data, E820 map at 0x8000, IDT at 0x10000,
    //         kernel at 0x1000, stack at 0x90000, VGA at 0xB8000
    identity_map_range(0x00000, 0x400000, PTE_PRESENT | PTE_WRITABLE);    
    // Also identity map the page directory itself and any page tables
    // PMM allocates from above 1MB, so we need to map those frames too
    map_page((uint32_t)page_directory, (uint32_t)page_directory, PTE_PRESENT | PTE_WRITABLE);

    // Map all page tables that were allocated during identity_map_range
    for (int i = 0; i < 1024; i++) {
        if (page_directory[i] & PTE_PRESENT) {
            uint32_t table_addr = page_directory[i] & 0xFFFFF000;
            map_page(table_addr, table_addr, PTE_PRESENT | PTE_WRITABLE);
        }
    }

    // Register the page fault handler on ISR 14
    register_interrupt_handler(14, page_fault_handler);

    // Load page directory into CR3 and enable paging
    __asm__ volatile(
        "mov %0, %%cr3\n"          // Load page directory base address
        "mov %%cr0, %%eax\n"       // Read current CR0
        "or $0x80000000, %%eax\n"  // Set PG bit (bit 31)
        "mov %%eax, %%cr0\n"       // Paging is now ON
        :
        : "r"(page_directory)
        : "eax"
    );
}