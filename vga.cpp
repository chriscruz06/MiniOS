#include "vga.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static uint16_t* vga_buffer;
static int cursor_x;
static int cursor_y;
static uint8_t current_color;

void vga_init() {
    vga_buffer = (uint16_t*)VGA_MEMORY;
    cursor_x = 0;
    cursor_y = 0;
    current_color = (VGA_BLACK << 4) | VGA_LIGHT_GREY;
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = (bg << 4) | fg;
}

static void vga_scroll() {
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++) {
        vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
    }
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (current_color << 8) | ' ';
    }
    cursor_y = VGA_HEIGHT - 1;
}

void vga_put_char(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (current_color << 8) | ' ';
        }
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;  // Align to next 8-column boundary
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (current_color << 8) | c;
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_print(const char* str) {
    while (*str) {
        vga_put_char(*str++);
    }
}

void vga_clear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (current_color << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
}

void vga_print_hex(uint32_t value) {
    vga_print("0x");
    char hex_chars[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        vga_put_char(hex_chars[(value >> i) & 0xF]);
    }
}

void vga_print_int(int value) {
    if (value < 0) {
        vga_put_char('-');
        value = -value;
    }
    if (value == 0) {
        vga_put_char('0');
        return;
    }
    
    char buf[12];
    int i = 0;
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    while (i > 0) {
        vga_put_char(buf[--i]);
    }
}

void vga_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
}

int vga_get_cursor_x() { return cursor_x; }
int vga_get_cursor_y() { return cursor_y; }