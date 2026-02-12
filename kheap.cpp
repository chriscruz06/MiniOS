#include "kheap.h"
#include "pmm.h"
#include "paging.h"

// ============================================================================
// Heap configuration
// ============================================================================

// Heap lives at virtual address 4MB and grows upward
// Physical frames are allocated from PMM and mapped into this range
#define HEAP_START      0x400000
#define HEAP_INITIAL_PAGES  4       // Start with 16KB
#define HEAP_MAX_PAGES      256     // Max 1MB heap

// ============================================================================
// Block header sits right before every allocation
// ============================================================================

struct BlockHeader {
    uint32_t    size;       // Size of usable data (NOT including header)
    bool        free;       // Asks "Is this block free?"
    BlockHeader* next;      // Next block in the list
    BlockHeader* prev;      // Previous block in the list
    uint32_t    magic;      // Sanity check: 0xDEADBEEF = valid block
};

#define HEADER_SIZE     sizeof(BlockHeader)
#define BLOCK_MAGIC     0xDEADBEEF
#define MIN_BLOCK_SIZE  8   // Minimum usable size worth splitting for

// ============================================================================
// Heap state
// ============================================================================

static BlockHeader* heap_start_block;
static uint32_t heap_vaddr_end;         // Next unmapped virtual address
static uint32_t heap_pages_used;

// ============================================================================
// Expand the heap by mapping more pages
// ============================================================================

static bool heap_expand(uint32_t pages) {
    for (uint32_t i = 0; i < pages; i++) {
        if (heap_pages_used >= HEAP_MAX_PAGES) {
            return false;
        }

        void* frame = pmm_alloc_frame();
        if (!frame) {
            return false;
        }

        // Map the physical frame to the next virtual address in our heap range
        map_page(heap_vaddr_end, (uint32_t)frame, PTE_PRESENT | PTE_WRITABLE);
        heap_vaddr_end += PAGE_SIZE;
        heap_pages_used++;
    }

    return true;
}

// ============================================================================
// Initialize the kernel heap
// ============================================================================

void kheap_init() {
    heap_start_block = 0;
    heap_vaddr_end = HEAP_START;
    heap_pages_used = 0;

    // Allocate initial pages
    if (!heap_expand(HEAP_INITIAL_PAGES)) {
        // Can't even get initial heap memory, it was rigged from the start
        return;
    }

    // Set up the first free block spanning the entire initial heap
    heap_start_block = (BlockHeader*)HEAP_START;
    heap_start_block->size = (HEAP_INITIAL_PAGES * PAGE_SIZE) - HEADER_SIZE;
    heap_start_block->free = true;
    heap_start_block->next = 0;
    heap_start_block->prev = 0;
    heap_start_block->magic = BLOCK_MAGIC;
}

// ============================================================================
// Find a free block (first-fit)
// ============================================================================

static BlockHeader* find_free_block(uint32_t size) {
    BlockHeader* current = heap_start_block;

    while (current) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }

    return 0;
}

// ============================================================================
// Split a block if theres enough leftover space
// ============================================================================

static void split_block(BlockHeader* block, uint32_t size) {
    uint32_t remaining = block->size - size - HEADER_SIZE;

    // Only split if the remainder is big enough to be useful
    if (remaining < MIN_BLOCK_SIZE) {
        return;
    }

    // Create a new free block after the allocated portion
    BlockHeader* new_block = (BlockHeader*)((uint8_t*)block + HEADER_SIZE + size);
    new_block->size = remaining;
    new_block->free = true;
    new_block->magic = BLOCK_MAGIC;

    // Insert into linked list
    new_block->next = block->next;
    new_block->prev = block;
    if (block->next) {
        block->next->prev = new_block;
    }
    block->next = new_block;

    // Shrink the original block
    block->size = size;
}

// ============================================================================
// kmalloc: allocate size bytes
// ============================================================================

void* kmalloc(uint32_t size) {
    if (size == 0) {
        return 0;
    }

    // Align to 4 bytes for sanity
    size = (size + 3) & ~3;

    // Try to find a free block
    BlockHeader* block = find_free_block(size);

    // If no block found, try expanding the heap
    if (!block) {
        // Figure out how many pages needed
        uint32_t bytes_needed = size + HEADER_SIZE;
        uint32_t pages_needed = (bytes_needed + PAGE_SIZE - 1) / PAGE_SIZE;
        if (pages_needed < 2) pages_needed = 2;  // Expand by at least 2 pages

        // Find the last block to extend or append
        BlockHeader* last = heap_start_block;
        while (last && last->next) {
            last = last->next;
        }

        uint32_t old_end = heap_vaddr_end;
        if (!heap_expand(pages_needed)) {
            return 0;  // OOM
        }

        // If last block is free, extend it into the new pages
        if (last && last->free) {
            // Check if last block is adjacent to the new memory
            uint32_t last_end = (uint32_t)last + HEADER_SIZE + last->size;
            if (last_end == old_end) {
                last->size += pages_needed * PAGE_SIZE;
                block = last;
            }
        }

        // If couldn't extend, create a new block at the old end
        if (!block) {
            BlockHeader* new_block = (BlockHeader*)old_end;
            new_block->size = (pages_needed * PAGE_SIZE) - HEADER_SIZE;
            new_block->free = true;
            new_block->magic = BLOCK_MAGIC;
            new_block->prev = last;
            new_block->next = 0;
            if (last) {
                last->next = new_block;
            }
            block = new_block;
        }
    }

    // Mark as used and split if possible
    block->free = false;
    split_block(block, size);

    // Return pointer to usable memory (right after the header)
    return (void*)((uint8_t*)block + HEADER_SIZE);
}

// ============================================================================
// Coalesce (pulled out the thesaurus for this one): merge adjacent free blocks
// ============================================================================

static void coalesce(BlockHeader* block) {
    // Merge with next block if it's free
    if (block->next && block->next->free) {
        // Verify next block is actually adjacent in memory
        uint32_t expected = (uint32_t)block + HEADER_SIZE + block->size;
        if (expected == (uint32_t)block->next) {
            block->size += HEADER_SIZE + block->next->size;
            block->next = block->next->next;
            if (block->next) {
                block->next->prev = block;
            }
        }
    }

    // Merge with previous block if its free
    if (block->prev && block->prev->free) {
        uint32_t expected = (uint32_t)block->prev + HEADER_SIZE + block->prev->size;
        if (expected == (uint32_t)block) {
            block->prev->size += HEADER_SIZE + block->size;
            block->prev->next = block->next;
            if (block->next) {
                block->next->prev = block->prev;
            }
        }
    }
}

// ============================================================================
// kfree: free a previously allocated pointer
// ============================================================================

void kfree(void* ptr) {
    if (!ptr) {
        return;
    }

    // Get the block header (sits right before the pointer)
    BlockHeader* block = (BlockHeader*)((uint8_t*)ptr - HEADER_SIZE);

    // Sanity check
    if (block->magic != BLOCK_MAGIC) {
        // Bad pointer or heap corruption - just bail
        return;
    }

    if (block->free) {
        // Double free == ignore
        return;
    }

    block->free = true;

    // Try to merge with neighbors
    coalesce(block);
}

// ============================================================================
// Stats n stuff
// ============================================================================

uint32_t kheap_get_total_bytes() {
    return heap_pages_used * PAGE_SIZE;
}

uint32_t kheap_get_used_bytes() {
    uint32_t used = 0;
    BlockHeader* current = heap_start_block;
    while (current) {
        if (!current->free) {
            used += current->size + HEADER_SIZE;
        }
        current = current->next;
    }
    return used;
}

uint32_t kheap_get_free_bytes() {
    return kheap_get_total_bytes() - kheap_get_used_bytes();
}

uint32_t kheap_get_block_count() {
    uint32_t count = 0;
    BlockHeader* current = heap_start_block;
    while (current) {
        count++;
        current = current->next;
    }
    return count;
}

// ============================================================================
// Debug dump
// ============================================================================

void kheap_dump() {
    // Uses VGA directly for kernel level debug
    uint16_t* vga = (uint16_t*)0xb8000;
    const char* hex = "0123456789ABCDEF";

    int row = 5;
    BlockHeader* current = heap_start_block;
    int block_num = 0;

    while (current && row < 24) {
        int col = 0;
        int idx = row * 80;

        // Block number
        vga[idx + col++] = (0x0E << 8) | '#';
        vga[idx + col++] = (0x0E << 8) | ('0' + block_num % 10);
        vga[idx + col++] = (0x07 << 8) | ' ';

        // Address
        uint32_t addr = (uint32_t)current;
        for (int i = 7; i >= 0; i--) {
            vga[idx + col++] = (0x0B << 8) | hex[(addr >> (i * 4)) & 0xF];
        }
        vga[idx + col++] = (0x07 << 8) | ' ';

        // Size
        uint32_t sz = current->size;
        for (int i = 7; i >= 0; i--) {
            vga[idx + col++] = (0x0F << 8) | hex[(sz >> (i * 4)) & 0xF];
        }
        vga[idx + col++] = (0x07 << 8) | ' ';

        // Free/Used
        uint8_t color = current->free ? 0x0A : 0x0C;
        const char* status = current->free ? "FREE" : "USED";
        for (int i = 0; status[i]; i++) {
            vga[idx + col++] = (color << 8) | status[i];
        }

        row++;
        block_num++;
        current = current->next;
    }
}