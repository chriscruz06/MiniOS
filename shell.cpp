#include "shell.h"
#include "vga.h"
#include "timer.h"
#include "sleep.h"
#include "ports.h"
#include "keyboard.h"
#include "pmm.h"
#include "kheap.h"
#include "ata.h"
#include "fat16.h"

#define CMD_BUFFER_SIZE 256
#define HISTORY_SIZE 10

static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_index;
static int cmd_cursor;
static uint8_t prompt_color;

// Command history
static char history[HISTORY_SIZE][CMD_BUFFER_SIZE];
static int history_count;
static int history_index; 

// String utilities
static int str_eq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return *a == *b;
}

static int str_starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str++ != *prefix++) return 0;
    }
    return 1;
}

static int str_len(const char* s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

static void str_copy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static const char* skip_spaces(const char* s) {
    while (*s == ' ') s++;
    return s;
}

static int parse_int(const char* s) {
    int result = 0;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result;
}

// Clear current input line and reprint
static void shell_redraw_line() {
    // Move to start of input (after prompt)
    vga_print("\r");
    vga_set_color(prompt_color, VGA_BLACK);
    vga_print("chris@minios");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("> ");
    
    // Print current buffer
    cmd_buffer[cmd_index] = '\0';
    vga_print(cmd_buffer);
    
    // Clear rest of line
    for (int i = cmd_index; i < 60; i++) {
        vga_put_char(' ');
    }
    
    // Reposition cursor
    vga_print("\r");
    vga_set_color(prompt_color, VGA_BLACK);
    vga_print("chris@minios");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("> ");
    for (int i = 0; i < cmd_cursor; i++) {
        vga_put_char(cmd_buffer[i]);
    }
}

static void shell_prompt() {
    vga_set_color(prompt_color, VGA_BLACK);
    vga_print("chris@minios");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("> ");
}

static void history_add(const char* cmd) {
    if (str_len(cmd) == 0) return;
    
    // Don't add duplicates of last command
    if (history_count > 0 && str_eq(history[history_count - 1], cmd)) {
        return;
    }
    
    // Shift history if full
    if (history_count >= HISTORY_SIZE) {
        for (int i = 0; i < HISTORY_SIZE - 1; i++) {
            str_copy(history[i], history[i + 1]);
        }
        history_count = HISTORY_SIZE - 1;
    }
    
    str_copy(history[history_count], cmd);
    history_count++;
}

// Commands
static void cmd_help() {
    vga_print("Available commands:\n");
    vga_print("  help          - Show this message\n");
    vga_print("  clear         - Clear the screen\n");
    vga_print("  echo <text>   - Print text\n");
    vga_print("  ticks         - Show timer ticks\n");
    vga_print("  uptime        - Show system uptime\n");
    vga_print("  about         - System information\n");
    vga_print("  color <fg>    - Set prompt color (0-15)\n");
    vga_print("  colors        - Show all colors\n");
    vga_print("  memmap        - Show E820 memory map & PMM stats\n");
    vga_print("  memtest       - Allocate and free page frames\n");
    vga_print("  heap          - Show kernel heap stats\n");
    vga_print("  heaptest      - Test kmalloc/kfree\n");
    vga_print("  disktest      - Test ATA disk driver\n");
    vga_print("  ls            - List files on disk\n");
    vga_print("  cat <file>    - Display file contents\n");
    vga_print("  write <f> <t> - Create file with text\n");
    vga_print("  touch <file>  - Create empty file\n");
    vga_print("  rm <file>     - Delete a file\n");
    vga_print("  mkdir <name>  - Create a directory\n");
}

static void cmd_echo(const char* args) {
    args = skip_spaces(args);
    vga_print(args);
    vga_put_char('\n');
}

static void cmd_uptime() {
    uint32_t ticks = timer_get_ticks();
    uint32_t seconds = ticks / 100;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    
    vga_print("Uptime: ");
    if (hours > 0) {
        vga_print_int(hours);
        vga_print("h ");
    }
    if (minutes > 0 || hours > 0) {
        vga_print_int(minutes % 60);
        vga_print("m ");
    }
    vga_print_int(seconds % 60);
    vga_print("s (");
    vga_print_int(ticks);
    vga_print(" ticks)\n");
}

static void cmd_about() {
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    vga_print("\n  MiniOS v0.3\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  A bare-metal x86 operating system\n");
    vga_print("  Built from scratch as a learning project\n\n");
    vga_print("  Features:\n");
    vga_print("   - Protected mode (32-bit)\n");
    vga_print("   - IDT & ISR interrupt handling\n");
    vga_print("   - PIC with timer (100Hz) & keyboard\n");
    vga_print("   - VGA text mode driver\n");
    vga_print("   - Interactive shell with history\n");
    vga_print("   - Physical memory manager (E820 + bitmap)\n");
    vga_print("   - Virtual memory / paging\n");
    vga_print("   - Kernel heap (kmalloc/kfree)\n");
    vga_print("   - ATA PIO disk driver\n");
    vga_print("   - FAT16 filesystem (read/write/delete)\n\n");
}
static void cmd_colors() {
    vga_print("Available colors:\n");
    const char* color_names[] = {
        "0:black", "1:blue", "2:green", "3:cyan",
        "4:red", "5:magenta", "6:brown", "7:lgrey",
        "8:dgrey", "9:lblue", "10:lgreen", "11:lcyan",
        "12:lred", "13:lmagenta", "14:yellow", "15:white"
    };
    for (int i = 0; i < 16; i++) {
        vga_set_color(i, VGA_BLACK);
        vga_print("  ");
        vga_print(color_names[i]);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        if (i % 4 == 3) vga_put_char('\n');
    }
}

static void cmd_color(const char* args) {
    args = skip_spaces(args);
    if (*args == '\0') {
        vga_print("Usage: color <0-15>\n");
        vga_print("Type 'colors' to see options\n");
        return;
    }
    int fg = parse_int(args);
    if (fg < 0 || fg > 15) {
        vga_print("Color must be 0-15\n");
        return;
    }
    prompt_color = fg;
    vga_print("Prompt color set!\n");
}

static void cmd_memtest() {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("Allocating 5 page frames...\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    
    void* frames[5];
    for (int i = 0; i < 5; i++) {
        frames[i] = pmm_alloc_frame();
        vga_print("  Frame ");
        vga_print_int(i);
        vga_print(": ");
        if (frames[i]) {
            vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
            vga_print_hex((uint32_t)frames[i]);
        } else {
            vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
            vga_print("FAILED");
        }
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_put_char('\n');
    }
    
    vga_print("  Free frames: ");
    vga_print_int(pmm_get_free_frames());
    vga_put_char('\n');
    
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("Freeing all frames...\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    
    for (int i = 0; i < 5; i++) {
        if (frames[i]) {
            pmm_free_frame(frames[i]);
        }
    }
    
    vga_print("  Free frames: ");
    vga_print_int(pmm_get_free_frames());
    vga_print(" (should match before alloc)\n");
}

static void cmd_heap() {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("Kernel Heap Stats:\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    
    vga_print("  Total: ");
    vga_print_int(kheap_get_total_bytes());
    vga_print(" bytes (");
    vga_print_int(kheap_get_total_bytes() / 1024);
    vga_print(" KB)\n");
    
    vga_print("  Used:  ");
    vga_print_int(kheap_get_used_bytes());
    vga_print(" bytes\n");
    
    vga_print("  Free:  ");
    vga_print_int(kheap_get_free_bytes());
    vga_print(" bytes\n");
    
    vga_print("  Blocks: ");
    vga_print_int(kheap_get_block_count());
    vga_put_char('\n');
}

static void cmd_heaptest() {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("Heap allocation test...\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    
    // Show initial state
    vga_print("  Before: ");
    vga_print_int(kheap_get_free_bytes());
    vga_print(" bytes free, ");
    vga_print_int(kheap_get_block_count());
    vga_print(" blocks\n");
    
    // Allocate a few things
    vga_print("  Allocating 64, 128, 256 bytes...\n");
    void* a = kmalloc(64);
    void* b = kmalloc(128);
    void* c = kmalloc(256);
    
    vga_print("    a=");
    vga_print_hex((uint32_t)a);
    vga_print("  b=");
    vga_print_hex((uint32_t)b);
    vga_print("  c=");
    vga_print_hex((uint32_t)c);
    vga_put_char('\n');
    
    // Verify they're in heap range
    bool ok = a && b && c;
    if (ok) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("  Allocations OK\n");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("  ALLOCATION FAILED\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return;
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    
    // Free middle block
    vga_print("  Freeing b (128 bytes)...\n");
    kfree(b);
    
    vga_print("  After free: ");
    vga_print_int(kheap_get_block_count());
    vga_print(" blocks\n");
    
    // Reallocate into freed space
    vga_print("  Allocating 100 bytes (should reuse b's slot)...\n");
    void* d = kmalloc(100);
    vga_print("    d=");
    vga_print_hex((uint32_t)d);
    vga_put_char('\n');
    
    // Free everything
    vga_print("  Freeing all...\n");
    kfree(a);
    kfree(c);
    kfree(d);
    
    // Final state - should coalesce back to 1 block
    vga_print("  After: ");
    vga_print_int(kheap_get_free_bytes());
    vga_print(" bytes free, ");
    vga_print_int(kheap_get_block_count());
    vga_print(" blocks");
    
    if (kheap_get_block_count() == 1) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print(" (coalesced!)\n");
    } else {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_print("\n");
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void cmd_disktest() {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("ATA Disk Driver Test\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    // Step 1: Detect the drive
    vga_print("  Detecting drive... ");
    int result = ata_init();
    if (result != 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("FAILED (error ");
        vga_print_int(result);
        vga_print(")\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return;
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("OK\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    // Step 2: Read sector 0 (FAT16 boot sector / BPB)
    uint8_t buf[512];
    vga_print("  Reading sector 0... ");
    result = ata_read_sectors(0, 1, buf);
    if (result != 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("FAILED (error ");
        vga_print_int(result);
        vga_print(")\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return;
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("OK\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    // Step 3: Check boot signature at bytes 510-511
    vga_print("  Boot signature: 0x");
    vga_print_hex(buf[511]);
    vga_print_hex(buf[510]);
    if (buf[510] == 0x55 && buf[511] == 0xAA) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print(" VALID\n");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print(" INVALID\n");
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    // Step 4: Print OEM name from BPB (bytes 3-10)
    vga_print("  OEM Name: ");
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    for (int i = 3; i < 11; i++) {
        char c = buf[i];
        if (c >= 32 && c < 127) {
            vga_put_char(c);
        }
    }
    vga_put_char('\n');
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    // Step 5: Print some FAT16 BPB fields
    uint16_t bytes_per_sector = buf[11] | (buf[12] << 8);
    uint8_t sectors_per_cluster = buf[13];
    uint16_t reserved_sectors = buf[14] | (buf[15] << 8);
    uint8_t num_fats = buf[16];
    uint16_t root_entry_count = buf[17] | (buf[18] << 8);
    uint16_t total_sectors_16 = buf[19] | (buf[20] << 8);

    vga_print("  Bytes/sector:     ");
    vga_print_int(bytes_per_sector);
    vga_put_char('\n');
    vga_print("  Sectors/cluster:  ");
    vga_print_int(sectors_per_cluster);
    vga_put_char('\n');
    vga_print("  Reserved sectors: ");
    vga_print_int(reserved_sectors);
    vga_put_char('\n');
    vga_print("  Number of FATs:   ");
    vga_print_int(num_fats);
    vga_put_char('\n');
    vga_print("  Root entries:     ");
    vga_print_int(root_entry_count);
    vga_put_char('\n');
    vga_print("  Total sectors:    ");
    vga_print_int(total_sectors_16);
    vga_put_char('\n');

    // Step 6: Read-write-read test on a high sector to avoid trashing the FAT
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_print("  Write/read test...\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    uint8_t write_buf[512];
    uint8_t read_buf[512];
    // Fill with a recognizable pattern
    for (int i = 0; i < 512; i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }

    // Use a high sector number to avoid corrupting FAT/root dir
    uint32_t test_sector = 1000;

    vga_print("    Writing sector ");
    vga_print_int(test_sector);
    vga_print("... ");
    result = ata_write_sectors(test_sector, write_buf, 1);
    if (result != 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("FAILED\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return;
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("OK\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    vga_print("    Reading it back... ");
    result = ata_read_sectors(test_sector, 1, read_buf);
    if (result != 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("FAILED\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return;
    }
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print("OK\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    // Verify data matches
    vga_print("    Verifying data... ");
    bool match = true;
    for (int i = 0; i < 512; i++) {
        if (read_buf[i] != write_buf[i]) {
            match = false;
            break;
        }
    }
    if (match) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("PASS - all 512 bytes match!\n");
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("FAIL - data mismatch!\n");
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void cmd_ls() {
    fat16_list_root();
}

static void cmd_cat(const char* args) {
    args = skip_spaces(args);
    if (*args == '\0') {
        vga_print("Usage: cat <filename>\n");
        return;
    }
    
    // Get file size first
    int size = fat16_file_size(args);
    if (size < 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: ");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(args);
        vga_put_char('\n');
        return;
    }
    
    if (size == 0) {
        vga_print("(empty file)\n");
        return;
    }
    
    // Cap at 4KB for display - don't want to flood the screen
    uint32_t read_size = (uint32_t)size;
    if (read_size > 4096) read_size = 4096;
    
    // Allocate buffer from heap
    uint8_t* buf = (uint8_t*)kmalloc(read_size + 1);
    if (!buf) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Out of memory\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return;
    }
    
    int bytes_read = fat16_read_file(args, buf, read_size);
    if (bytes_read < 0) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Error reading file\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        kfree(buf);
        return;
    }
    
    // Null-terminate and print
    buf[bytes_read] = '\0';
    
    // Print character by character (handles non-printable chars)
    for (int i = 0; i < bytes_read; i++) {
        char c = buf[i];
        if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < bytes_read && buf[i + 1] == '\n') {
                i++;  // Skip \n in \r\n pair
            }
            vga_put_char('\n');
        } else if (c >= 32 && c < 127) {
            vga_put_char(c);
        } else if (c == '\t') {
            vga_print("    ");
        }
    }
    vga_put_char('\n');
    
    if ((uint32_t)size > 4096) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_print("(truncated - file is ");
        vga_print_int(size);
        vga_print(" bytes)\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    }
    
    kfree(buf);
}

static void cmd_write(const char* args) {
    args = skip_spaces(args);
    if (*args == '\0') {
        vga_print("Usage: write <filename> <text>\n");
        return;
    }
    
    // Parse filename (first word)
    char filename[64];
    int fi = 0;
    while (*args && *args != ' ' && fi < 63) {
        filename[fi++] = *args++;
    }
    filename[fi] = '\0';
    
    // Rest is the content
    args = skip_spaces(args);
    if (*args == '\0') {
        vga_print("Usage: write <filename> <text>\n");
        return;
    }
    
    int len = str_len(args);
    
    int result = fat16_create_file(filename, args, (uint32_t)len);
    if (result == 0) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("Wrote ");
        vga_print_int(len);
        vga_print(" bytes to ");
        vga_print(filename);
        vga_put_char('\n');
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Failed to write file\n");
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void cmd_touch(const char* args) {
    args = skip_spaces(args);
    if (*args == '\0') {
        vga_print("Usage: touch <filename>\n");
        return;
    }
    
    // Check if file already exists
    int size = fat16_file_size(args);
    if (size >= 0) {
        vga_print("File already exists (");
        vga_print_int(size);
        vga_print(" bytes)\n");
        return;
    }
    
    int result = fat16_create_file(args, nullptr, 0);
    if (result == 0) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("Created ");
        vga_print(args);
        vga_put_char('\n');
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Failed to create file\n");
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void cmd_rm(const char* args) {
    args = skip_spaces(args);
    if (*args == '\0') {
        vga_print("Usage: rm <filename>\n");
        return;
    }
    
    int result = fat16_delete(args);
    if (result == 0) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("Deleted ");
        vga_print(args);
        vga_put_char('\n');
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("File not found: ");
        vga_print(args);
        vga_put_char('\n');
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void cmd_mkdir(const char* args) {
    args = skip_spaces(args);
    if (*args == '\0') {
        vga_print("Usage: mkdir <dirname>\n");
        return;
    }
    
    int result = fat16_mkdir(args);
    if (result == 0) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_print("Created directory ");
        vga_print(args);
        vga_put_char('\n');
    } else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Failed (already exists or disk full)\n");
    }
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void shell_execute() {
    cmd_buffer[cmd_index] = '\0';
    
    const char* cmd = skip_spaces(cmd_buffer);
    
    // Add to history before executing
    history_add(cmd);
    history_index = history_count;
    
    if (*cmd == '\0') {
        // Empty command
    }
    else if (str_eq(cmd, "help")) {
        cmd_help();
    }
    else if (str_eq(cmd, "clear")) {
        vga_clear();
    }
    else if (str_eq(cmd, "heap")) {
        cmd_heap();
    }
    else if (str_eq(cmd, "heaptest")) {
        cmd_heaptest();
    }
    else if (str_starts_with(cmd, "echo ")) {
        cmd_echo(cmd + 5);
    }
    else if (str_eq(cmd, "echo")) {
        vga_put_char('\n');
    }
    else if (str_eq(cmd, "ticks")) {
        vga_print("Timer ticks: ");
        vga_print_int(timer_get_ticks());
        vga_put_char('\n');
    }
    else if (str_eq(cmd, "uptime")) {
        cmd_uptime();
    }
    else if (str_eq(cmd, "about")) {
        cmd_about();
    }
    else if (str_eq(cmd, "colors")) {
        cmd_colors();
    }
    else if (str_starts_with(cmd, "color ")) {
        cmd_color(cmd + 6);
    }
    else if (str_eq(cmd, "color")) {
        cmd_color("");
    }
    else if (str_eq(cmd, "memmap")) {
        pmm_dump();
    }
    else if (str_eq(cmd, "memtest")) {
        cmd_memtest();
    }
    else if (str_eq(cmd, "disktest")) {
        cmd_disktest();
    }
    else if (str_eq(cmd, "ls")) {
        cmd_ls();
    }
    else if (str_starts_with(cmd, "cat ")) {
        cmd_cat(cmd + 4);
    }
    else if (str_starts_with(cmd, "write ")) {
        cmd_write(cmd + 6);
    }
    else if (str_starts_with(cmd, "touch ")) {
        cmd_touch(cmd + 6);
    }
    else if (str_starts_with(cmd, "rm ")) {
        cmd_rm(cmd + 3);
    }
    else if (str_starts_with(cmd, "mkdir ")) {
        cmd_mkdir(cmd + 6);
    }
    else {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_print("Unknown command: ");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        vga_print(cmd);
        vga_print("\nType 'help' for available commands.\n");
    }
    
    cmd_index = 0;
    cmd_cursor = 0;
    shell_prompt();
}

void shell_init() {
    cmd_index = 0;
    cmd_cursor = 0;
    prompt_color = VGA_LIGHT_GREEN;
    history_count = 0;
    history_index = 0;
    
    vga_init();
    vga_clear();
    
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_print(" __  __ _       _  ___  ____  \n");
    vga_print("|  \\/  (_)_ __ (_)/ _ \\/ ___| \n");
    vga_print("| |\\/| | | '_ \\| | | | \\___ \\ \n");
    vga_print("| |  | | | | | | | |_| |___) |\n");
    vga_print("|_|  |_|_|_| |_|_|\\___/|____/ \n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("\nType 'help' for available commands.\n\n");
    
    shell_prompt();
}

void shell_handle_key(uint8_t c) {
    if (c == '\n') {
        vga_put_char('\n');
        shell_execute();
    }
    else if (c == '\b') {
        if (cmd_cursor > 0) {
            for (int i = cmd_cursor - 1; i < cmd_index - 1; i++) {
                cmd_buffer[i] = cmd_buffer[i + 1];
            }
            cmd_index--;
            cmd_cursor--;
            shell_redraw_line();
        }
    }
    else if (c == KEY_UP) {
        if (history_index > 0) {
            history_index--;
            str_copy(cmd_buffer, history[history_index]);
            cmd_index = str_len(cmd_buffer);
            cmd_cursor = cmd_index;
            shell_redraw_line();
        }
    }
    else if (c == KEY_DOWN) {
        if (history_index < history_count - 1) {
            history_index++;
            str_copy(cmd_buffer, history[history_index]);
            cmd_index = str_len(cmd_buffer);
            cmd_cursor = cmd_index;
            shell_redraw_line();
        }
        else if (history_index < history_count) {
            history_index = history_count;
            cmd_index = 0;
            cmd_cursor = 0;
            cmd_buffer[0] = '\0';
            shell_redraw_line();
        }
    }
    else if (c == KEY_LEFT) {
        if (cmd_cursor > 0) {
            cmd_cursor--;
            vga_print("\b"); 
        }
    }
    else if (c == KEY_RIGHT) {
        if (cmd_cursor < cmd_index) {
            vga_put_char(cmd_buffer[cmd_cursor]);
            cmd_cursor++;
        }
    }
    else if (c >= ' ' && c <= '~') {
        if (cmd_index < CMD_BUFFER_SIZE - 1) {
            // Insert at cursor position
            for (int i = cmd_index; i > cmd_cursor; i--) {
                cmd_buffer[i] = cmd_buffer[i - 1];
            }
            cmd_buffer[cmd_cursor] = c;
            cmd_index++;
            cmd_cursor++;
            shell_redraw_line();
        }
    }
}