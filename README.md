# MiniOS

A custom, bare metal, operating system built from scratch as a learning project.  
Built with hopes and dreams.
Heavily in progress.

## Features

- **Bootloader**: Custom x86 bootloader with multi-sector CHS disk loading and E820 memory detection
- **Protected Mode**: Full 32-bit protected mode operation
- **GDT**: Global Descriptor Table implementation
- **IDT**: Complete Interrupt Descriptor Table with ISRs
- **PIC**: Programmable Interrupt Controller with remapping
- **PIT Timer**: Programmable Interval Timer at 100Hz with handler registration
- **Keyboard Driver**: PS/2 keyboard with shift, caps lock, and arrow key support
- **VGA Text Mode**: Full text driver with colors, scrolling, and cursor control
- **Sleep**: Timing functions (sleep_ms, sleep_ticks)
- **Physical Memory Manager**: E820 BIOS memory detection with bitmap-based frame allocation
- **Virtual Memory**: Paging with identity-mapped kernel space, page fault handler with debug output
- **Kernel Heap**: kmalloc/kfree with free-list allocator, block splitting, and coalescing
- **Interactive Shell**: Command-line interface with:
  - Command history (up/down arrows)
  - Cursor movement (left/right arrows)
  - Customizable prompt colors
  - Built-in commands: `help`, `clear`, `echo`, `ticks`, `uptime`, `about`, `color`, `colors`, `memmap`, `memtest`, `heap`, `heaptest`

## Project Structure

```
├── boot.asm           # Bootloader - CHS disk loading, E820 detection, GDT, protected mode
├── kernel_entry.asm   # Kernel entry point (assembly)
├── kernel.cpp         # Main kernel - init sequence
├── idt.asm / idt.cpp  # Interrupt Descriptor Table
├── isr.asm / isr.cpp  # Interrupt Service Routines
├── pic.cpp            # PIC controller
├── timer.cpp          # PIT timer driver
├── keyboard.cpp       # PS/2 keyboard driver
├── vga.cpp            # VGA text mode driver
├── shell.cpp          # Interactive command shell
├── sleep.cpp          # Sleep/delay functions
├── pmm.cpp            # Physical memory manager (bitmap allocator)
├── paging.cpp         # Virtual memory / paging
├── kheap.cpp          # Kernel heap (kmalloc/kfree)
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

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `clear` | Clear the screen |
| `echo <text>` | Print text to screen |
| `ticks` | Show raw timer ticks |
| `uptime` | Show formatted system uptime |
| `about` | Display system information |
| `color <0-15>` | Change prompt color |
| `colors` | Show available colors |
| `memmap` | Show E820 memory map & PMM stats |
| `memtest` | Allocate and free physical page frames |
| `heap` | Show kernel heap stats |
| `heaptest` | Test kmalloc/kfree with allocation, freeing, and coalescing |

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
- [x] Keyboard driver (with shift & caps lock)
- [x] Sleep functions
- [x] VGA text driver
- [x] Command shell with history
- [x] Physical memory manager (E820 + bitmap)
- [x] Virtual memory / paging
- [x] Kernel heap (kmalloc/kfree)
- [ ] File system support

## Resources

Built following various OS development tutorials and resources, including the OSDev Wiki and heavily jumpstarted by
"Daedalus Community"s OS Dev guide. Lots of google and reddit posts too.

## License

This project is for educational purposes.