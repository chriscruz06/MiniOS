#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

void shell_init();
void shell_handle_key(uint8_t c);  // Changed to uint8_t for special keys

#endif