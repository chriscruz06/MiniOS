#include "shell.h"
#include "vga.h"
#include "timer.h"
#include "sleep.h"
#include "ports.h"
#include "keyboard.h"

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
    vga_print("\n  ChrisOS / MiniOS v0.1\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  A bare-metal x86 operating system\n");
    vga_print("  Built from scratch as a learning project\n\n");
    vga_print("  Features:\n");
    vga_print("   - Protected mode (32-bit)\n");
    vga_print("   - IDT & ISR interrupt handling\n");
    vga_print("   - PIC with timer (100Hz) & keyboard\n");
    vga_print("   - VGA text mode driver\n");
    vga_print("   - Interactive shell\n\n");
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