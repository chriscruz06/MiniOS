#include "pmm.h"
#include "vga.h"


// Bitmap lives at 0x20000 — past IDT (0x10000) and descriptor (0x10800)
#define BITMAP_ADDR 0x20000

// Max 256MB support. 256MB / 4KB = 65536 frames = 8KB bitmap for nowish
#define MAX_MEMORY    (256 * 1024 * 1024)
#define MAX_FRAMES    (MAX_MEMORY / PAGE_SIZE)
#define BITMAP_SIZE   (MAX_FRAMES / 8)

static uint8_t* bitmap = (uint8_t*)BITMAP_ADDR;

static uint32_t total_frames = 0;
static uint32_t used_frames = 0;

// E820 map info (read from bootloader)
static uint32_t e820_count = 0;
static E820Entry* e820_entries = (E820Entry*)E820_ENTRIES_ADDR;


static inline void bitmap_set(uint32_t frame) {
    bitmap[frame / 8] |= (1 << (frame % 8));
}

static inline void bitmap_clear(uint32_t frame) {
    bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static inline bool bitmap_test(uint32_t frame) {
    return (bitmap[frame / 8] & (1 << (frame % 8))) != 0;
}



void pmm_init() {
    e820_count = *((uint32_t*)E820_COUNT_ADDR);
    
    // Find the highest usable address to determine total memory
    uint64_t max_addr = 0;
    for (uint32_t i = 0; i < e820_count; i++) {
        uint64_t end = e820_entries[i].base + e820_entries[i].length;
        if (end > max_addr) {
            max_addr = end;
        }
    }
    
    // Cap to our max supported memory
    if (max_addr > MAX_MEMORY) {
        max_addr = MAX_MEMORY;
    }
    
    total_frames = (uint32_t)(max_addr / PAGE_SIZE);
    
    // Step 1: Mark ALL frames as used initially
    for (uint32_t i = 0; i < total_frames / 8; i++) {
        bitmap[i] = 0xFF;
    }
    used_frames = total_frames;
    
    // Step 2: Free frames that E820 says are usable
    for (uint32_t i = 0; i < e820_count; i++) {
        if (e820_entries[i].type == E820_USABLE) {
            uint64_t base = e820_entries[i].base;
            uint64_t length = e820_entries[i].length;
            
            // Align base up to page boundary
            if (base % PAGE_SIZE != 0) {
                uint64_t offset = PAGE_SIZE - (base % PAGE_SIZE);
                if (offset >= length) continue;
                base += offset;
                length -= offset;
            }
            
            // Free each page in this region
            uint32_t start_frame = (uint32_t)(base / PAGE_SIZE);
            uint32_t num_frames = (uint32_t)(length / PAGE_SIZE);
            
            for (uint32_t f = start_frame; f < start_frame + num_frames && f < total_frames; f++) {
                bitmap_clear(f);
                used_frames--;
            }
        }
    }
    
    uint32_t reserved_frames_1mb = (1024 * 1024) / PAGE_SIZE;  // 256 frames
    for (uint32_t f = 0; f < reserved_frames_1mb && f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            used_frames++;
        }
    }
}

void* pmm_alloc_frame() {
    for (uint32_t i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_frames++;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return nullptr;  // Out of memory
}

void pmm_free_frame(void* frame) {
    uint32_t addr = (uint32_t)frame;
    uint32_t index = addr / PAGE_SIZE;
    
    if (index >= total_frames) return;
    if (index < 256) return;  // Don't allow freeing below 1MB
    
    if (bitmap_test(index)) {
        bitmap_clear(index);
        used_frames--;
    }
}

bool pmm_is_frame_allocated(void* frame) {
    uint32_t addr = (uint32_t)frame;
    uint32_t index = addr / PAGE_SIZE;
    
    if (index >= total_frames) return true;
    return bitmap_test(index);
}

uint32_t pmm_get_total_frames() {
    return total_frames;
}

uint32_t pmm_get_used_frames() {
    return used_frames;
}

uint32_t pmm_get_free_frames() {
    return total_frames - used_frames;
}

uint32_t pmm_get_total_memory_kb() {
    return (total_frames * PAGE_SIZE) / 1024;
}


static const char* e820_type_str(uint32_t type) {
    switch (type) {
        case E820_USABLE:           return "Usable";
        case E820_RESERVED:         return "Reserved";
        case E820_ACPI_RECLAIMABLE: return "ACPI Reclaim";
        case E820_ACPI_NVS:         return "ACPI NVS";
        case E820_BAD_MEMORY:       return "Bad Memory";
        default:                    return "Unknown";
    }
}

// Print a 64-bit hex value using VGA driver
static void print_hex64(uint64_t val) {
    uint32_t hi = (uint32_t)(val >> 32);
    uint32_t lo = (uint32_t)(val & 0xFFFFFFFF);
    
    if (hi != 0) {
        vga_print_hex(hi);
        const char* hex = "0123456789ABCDEF";
        char buf[9];
        for (int i = 7; i >= 0; i--) {
            buf[7 - i] = hex[(lo >> (i * 4)) & 0xF];
        }
        buf[8] = 0;
        vga_print(buf);
    } else {
        vga_print_hex(lo);
    }
}

void pmm_dump() {
    // Header
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("=== Physical Memory Manager ===\n");
    
    // E820 Map
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("E820 Memory Map (");
    vga_print_int(e820_count);
    vga_print(" entries):\n");
    
    for (uint32_t i = 0; i < e820_count && i < 10; i++) {
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print("  ");
        
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        print_hex64(e820_entries[i].base);
        
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(" - ");
        
        vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
        print_hex64(e820_entries[i].base + e820_entries[i].length - 1);
        
        vga_print(" ");
        
        uint32_t type = e820_entries[i].type;
        vga_set_color(type == E820_USABLE ? VGA_LIGHT_GREEN : VGA_LIGHT_RED, VGA_BLACK);
        vga_print(e820_type_str(type));
        
        // Size
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        uint64_t size_kb = e820_entries[i].length / 1024;
        vga_print(" (");
        if (size_kb >= 1024) {
            vga_print_int((int)(size_kb / 1024));
            vga_print(" MB");
        } else {
            vga_print_int((int)size_kb);
            vga_print(" KB");
        }
        vga_print(")\n");
    }
    
    vga_put_char('\n');
    
    // PMM Stats
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("PMM Stats:\n");
    
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  Total: ");
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_print_int(pmm_get_total_memory_kb() / 1024);
    vga_print(" MB");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print(" (");
    vga_print_int(total_frames);
    vga_print(" frames)\n");
    
    vga_print("  Used:  ");
    vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
    vga_print_int(used_frames);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print(" frames\n");
    
    vga_print("  Free:  ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print_int(pmm_get_free_frames());
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print(" frames\n");
    
    vga_print("  Bitmap at: ");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print_hex(BITMAP_ADDR);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_put_char('\n');
    
    // Alloc test
    vga_put_char('\n');
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("Alloc test: ");
    void* test = pmm_alloc_frame();
    if (test) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("OK @ ");
        vga_print_hex((uint32_t)test);
        pmm_free_frame(test);
        vga_print(" (freed)");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("FAILED - no free frames!");
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_put_char('\n');
}