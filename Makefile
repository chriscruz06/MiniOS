# MiniOS Makefile

# Tools
ASM = nasm
CC = i686-elf-g++
LD = i686-elf-ld
QEMU = qemu-system-i386

# Flags
CFLAGS = -ffreestanding -m32 -fno-exceptions -fno-rtti -c
LDFLAGS = -Ttext 0x10000 --oformat binary

# Source files
ASM_BOOT = boot.asm
ASM_ENTRY = kernel_entry.asm
ASM_ISR = isr.asm
ASM_IDT = idt.asm

CPP_SOURCES = kernel.cpp idt.cpp isr.cpp pic.cpp keyboard.cpp timer.cpp \
              vga.cpp shell.cpp sleep.cpp pmm.cpp paging.cpp kheap.cpp ata.cpp fat16.cpp

# Object files
OBJ_ENTRY = kernel_entry.o
OBJ_ISR_ASM = isr_asm.o
OBJ_IDT_ASM = idt_asm.o
OBJ_CPP = kernel.o idt.o isr.o pic.o keyboard.o timer.o \
          vga.o shell.o sleep.o pmm.o paging.o kheap.o ata.o fat16.o

ALL_OBJS = $(OBJ_ENTRY) $(OBJ_CPP) $(OBJ_IDT_ASM) $(OBJ_ISR_ASM)

# Output files
BOOT_BIN = boot.bin
KERNEL_BIN = kernel.bin
OS_IMAGE = OS.bin
DISK_IMG = disk.img

# Disk image size (16MB FAT16 partition)
DISK_SIZE_MB = 16

# Default target
all: $(OS_IMAGE)

# Run in QEMU - boot from floppy, attach hard disk for filesystem
run: $(OS_IMAGE) $(DISK_IMG)
	$(QEMU) -fda $(OS_IMAGE) -hda $(DISK_IMG) -boot a

# Create FAT16-formatted disk image (only if it doesn't exist)
$(DISK_IMG):
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=$(DISK_SIZE_MB)
	mkfs.fat -F 16 $(DISK_IMG)

# Force-recreate the disk image (wipes all data)
newdisk:
	rm -f $(DISK_IMG)
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=$(DISK_SIZE_MB)
	mkfs.fat -F 16 $(DISK_IMG)

# Create final OS image
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $(OS_IMAGE)
	truncate -s 1440K $(OS_IMAGE)

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

keyboard.o: keyboard.cpp keyboard.h isr.h ports.h shell.h
	$(CC) $(CFLAGS) $< -o $@

timer.o: timer.cpp timer.h isr.h ports.h
	$(CC) $(CFLAGS) $< -o $@

vga.o: vga.cpp vga.h
	$(CC) $(CFLAGS) $< -o $@

shell.o: shell.cpp shell.h vga.h timer.h sleep.h ports.h keyboard.h pmm.h kheap.h ata.h fat16.h
	$(CC) $(CFLAGS) $< -o $@

sleep.o: sleep.cpp sleep.h timer.h
	$(CC) $(CFLAGS) $< -o $@

pmm.o: pmm.cpp pmm.h vga.h
	$(CC) $(CFLAGS) $< -o $@

paging.o: paging.cpp paging.h isr.h
	$(CC) $(CFLAGS) $< -o $@

kheap.o: kheap.cpp kheap.h pmm.h paging.h
	$(CC) $(CFLAGS) $< -o $@

ata.o: ata.cpp ata.h ports.h
	$(CC) $(CFLAGS) $< -o $@

fat16.o: fat16.cpp fat16.h ata.h vga.h
	$(CC) $(CFLAGS) $< -o $@

# Clean build artifacts (preserves disk image)
clean:
	rm -f $(BOOT_BIN) $(KERNEL_BIN) $(OS_IMAGE) $(ALL_OBJS)

# Clean everything including disk image
cleanall: clean
	rm -f $(DISK_IMG)

# Rebuild everything
rebuild: clean all

.PHONY: all run clean cleanall rebuild newdisk