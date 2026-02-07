#include "keyboard.h"
#include "isr.h"
#include "ports.h"
#include "shell.h"

// Scan codes for special keys
#define SC_LSHIFT     0x2A
#define SC_RSHIFT     0x36
#define SC_LSHIFT_REL 0xAA
#define SC_RSHIFT_REL 0xB6
#define SC_UP         0x48
#define SC_DOWN       0x50
#define SC_LEFT       0x4B
#define SC_RIGHT      0x4D
#define SC_CAPSLOCK   0x3A

static bool shift_pressed;
static bool caps_on;

// Lower
static const char scancode_normal[] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' '
};

// upper
static const char scancode_shifted[] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' '
};

static void keyboard_callback(registers_t* regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);
    
    // Handle shift press/release
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_pressed = true;
        return;
    }
    if (scancode == SC_LSHIFT_REL || scancode == SC_RSHIFT_REL) {
        shift_pressed = false;
        return;
    }
    
    // Handle caps lock (toggle on press only)
    if (scancode == SC_CAPSLOCK) {
        caps_on = !caps_on;
        return;
    }
    
    // Handle arrow keys (extended scancodes)
    if (!(scancode & 0x80)) { 
        switch (scancode) {
            case SC_UP:
                shell_handle_key(KEY_UP);
                return;
            case SC_DOWN:
                shell_handle_key(KEY_DOWN);
                return;
            case SC_LEFT:
                shell_handle_key(KEY_LEFT);
                return;
            case SC_RIGHT:
                shell_handle_key(KEY_RIGHT);
                return;
        }
        
        if (scancode < sizeof(scancode_normal)) {
            char c;
            bool use_shift = shift_pressed;
            
            // Caps lock affects only letters
            if (caps_on && scancode_normal[scancode] >= 'a' && scancode_normal[scancode] <= 'z') {
                use_shift = !use_shift;
            }
            
            if (use_shift) {
                c = scancode_shifted[scancode];
            } else {
                c = scancode_normal[scancode];
            }
            
            if (c != 0) {
                shell_handle_key(c);
            }
        }
    }
}

void keyboard_init() {
    shift_pressed = false;
    caps_on = false;
    register_interrupt_handler(33, keyboard_callback);
}