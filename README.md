# MiniOS

A custom, bare metal, operating system built from scratch as a learning project.  
Built with hopes and dreams.
Heavily in progress.

## Features

- **Bootloader**: Custom x86 bootloader written in assembly (20-sector loading)
- **Protected Mode**: Full 32-bit protected mode operation
- **GDT**: Global Descriptor Table implementation
- **IDT**: Complete Interrupt Descriptor Table with ISRs
- **PIC**: Programmable Interrupt Controller with remapping
- **PIT Timer**: Programmable Interval Timer at 100Hz with handler registration
- **Keyboard Driver**: PS/2 keyboard input with scancode-to-ASCII conversion
- **VGA Text Mode**: Text output with scrolling support
- **Sleep**: Timing functions (sleep_ms, sleep_ticks)

## Project Structure

```
├── boot.asm           # Bootloader (first stage)
├── kernel_entry.asm   # Kernel entry point (assembly)
├── kernel.cpp         # Main kernel (C++)
├── idt.asm / idt.cpp  # Interrupt Descriptor Table
├── isr.asm / isr.cpp  # Interrupt Service Routines
├── pic.cpp            # PIC controller
├── timer.cpp          # PIT timer driver
├── keyboard.cpp       # PS/2 keyboard driver
├── sleep.cpp          # Sleep/delay functions
├── ports.h            # I/O port operations
└── Makefile           # Build automation
```

## Building

### Prerequisites

- NASM assembler
- i686-elf cross-compiler toolchain
- QEMU for testing 

### Build Commands

```bash
# Build everything
make

# Build and run in QEMU
make run

# Clean build artifacts
make clean

# Full rebuild
make rebuild
```

## Development Environment

- WSL (Ubuntu, Debian derivative) on Windows
- Cross-compilation with i686-elf-gcc toolchain
- QEMU emulator for testing

## Roadmap

- [x] Bootloader
- [x] Protected mode
- [x] IDT & ISRs
- [x] PIC remapping
- [x] Timer (IRQ0)
- [x] Keyboard driver
- [x] Sleep functions
- [ ] Command shell interface
- [ ] Memory management (physical & virtual)
- [ ] File system support

## Resources

Built following various OS development tutorials and resources, including the OSDev Wiki and heavily jumpstarted by 
"Daedalus Community"s OS Dev guide. Lots of google and reddit posts too.

## License

This project is for educational purposes.