# MiniOS Makefile

# Tools
ASM = nasm
CC = i686-elf-g++
LD = i686-elf-ld
QEMU = qemu-system-i386

# Flags
CFLAGS = -ffreestanding -m32 -fno-exceptions -fno-rtti -O0 -c
LDFLAGS = -Ttext 0x1000 --oformat binary

# Source files
ASM_BOOT = boot.asm
ASM_ENTRY = kernel_entry.asm
ASM_ISR = isr.asm
ASM_IDT = idt.asm

CPP_SOURCES = kernel.cpp idt.cpp isr.cpp pic.cpp keyboard.cpp timer.cpp sleep.cpp

# Object files
OBJ_ENTRY = kernel_entry.o
OBJ_ISR_ASM = isr_asm.o
OBJ_IDT_ASM = idt_asm.o
OBJ_CPP = kernel.o idt.o isr.o pic.o keyboard.o timer.o sleep.o

ALL_OBJS = $(OBJ_ENTRY) $(OBJ_CPP) $(OBJ_IDT_ASM) $(OBJ_ISR_ASM)

# Output files
BOOT_BIN = boot.bin
KERNEL_BIN = kernel.bin
OS_IMAGE = OS.bin

# Default target
all: $(OS_IMAGE)

# Run in QEMU
run: $(OS_IMAGE)
	$(QEMU) -fda $(OS_IMAGE)

# Create final OS image
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $(OS_IMAGE)

# Bootloader
$(BOOT_BIN): $(ASM_BOOT)
	$(ASM) -f bin $< -o $@

# Kernel binary
$(KERNEL_BIN): $(ALL_OBJS)
	$(LD) -o $@ $(LDFLAGS) $(ALL_OBJS)

# Assembly objects
$(OBJ_ENTRY): $(ASM_ENTRY)
	$(ASM) -f elf32 $< -o $@

$(OBJ_ISR_ASM): $(ASM_ISR)
	$(ASM) -f elf32 $< -o $@

$(OBJ_IDT_ASM): $(ASM_IDT)
	$(ASM) -f elf32 $< -o $@

# C++ objects
kernel.o: kernel.cpp
	$(CC) $(CFLAGS) $< -o $@

idt.o: idt.cpp idt.h ports.h
	$(CC) $(CFLAGS) $< -o $@

isr.o: isr.cpp isr.h ports.h
	$(CC) $(CFLAGS) $< -o $@

pic.o: pic.cpp pic.h ports.h
	$(CC) $(CFLAGS) $< -o $@

keyboard.o: keyboard.cpp keyboard.h isr.h ports.h
	$(CC) $(CFLAGS) $< -o $@

timer.o: timer.cpp timer.h isr.h ports.h
	$(CC) $(CFLAGS) $< -o $@

sleep.o: sleep.cpp sleep.h timer.h
	$(CC) $(CFLAGS) $< -o $@

# Clean build artifacts
clean:
	rm -f $(BOOT_BIN) $(KERNEL_BIN) $(OS_IMAGE) $(ALL_OBJS) $(OBJ_IDT_ASM) $(OBJ_ISR_ASM)

# Rebuild everything
rebuild: clean all

.PHONY: all run clean rebuild