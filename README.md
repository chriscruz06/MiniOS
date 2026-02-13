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
- **ATA PIO Disk Driver**: IDE controller communication with 28-bit LBA addressing, supporting read/write operations on drives up to 128GB
- **FAT16 Filesystem**: Full read/write FAT16 implementation with BPB parsing, cluster chain traversal, dual FAT table updates, file creation/deletion, and directory support
- **Interactive Shell**: Command-line interface with:
  - Command history (up/down arrows)
  - Cursor movement (left/right arrows)
  - Customizable prompt colors
  - File management: `ls`, `cat`, `write`, `touch`, `rm`, `mkdir`
  - System commands: `help`, `clear`, `echo`, `ticks`, `uptime`, `about`, `color`, `colors`
  - Memory diagnostics: `memmap`, `memtest`, `heap`, `heaptest`, `disktest`

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
├── ata.cpp            # ATA PIO disk driver
├── fat16.cpp          # FAT16 filesystem driver
├── ports.h            # I/O port operations (8-bit and 16-bit)
└── Makefile           # Build automation
```

## Building

### Prerequisites

- NASM assembler
- i686-elf cross-compiler toolchain
- QEMU for testing
- dosfstools (for creating FAT16 disk images)

### Build Commands

```bash
# Build everything
make

# Build and run in QEMU
make run

# Clean build artifacts (preserves disk image)
make clean

# Clean everything including disk image
make cleanall

# Full rebuild
make rebuild

# Create a fresh FAT16 disk image
make newdisk
```

### Adding Files to the Disk Image (from host)

```bash
mkdir -p /tmp/fatmnt
sudo mount -o loop disk.img /tmp/fatmnt
echo "Hello from MiniOS!" | sudo tee /tmp/fatmnt/HELLO.TXT
sudo umount /tmp/fatmnt
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
| `disktest` | Test ATA disk driver (detect, read, write/verify) |
| `ls` | List files and directories on disk |
| `cat <file>` | Display file contents |
| `write <file> <text>` | Create a file with the given text |
| `touch <file>` | Create an empty file |
| `rm <file>` | Delete a file |
| `mkdir <name>` | Create a directory |

## Memory Map

| Address | Contents |
|---------|----------|
| `0x1000` | IDT |
| `0x7C00` | Bootloader |
| `0x8000` | E820 memory map |
| `0x9000` | Real mode stack |
| `0x10000` | Kernel (~34KB, loaded from floppy) |
| `0x90000` | Protected mode stack |

## Architecture

The system boots from a 1.44MB floppy image and attaches a separate 16MB FAT16-formatted hard disk image for filesystem operations. The bootloader loads the kernel using BIOS INT 13h with CHS addressing (sector-by-sector to handle track boundaries), detects available memory via E820, then transitions to 32-bit protected mode.

The kernel initializes subsystems in dependency order: IDT/ISRs, PIC, keyboard, timer, ATA disk driver, FAT16 filesystem, physical memory manager, paging, heap allocator, and finally the interactive shell. Interrupts are enabled after all initialization is complete.

The FAT16 driver communicates with the disk through the ATA PIO driver, which talks directly to the IDE controller via I/O ports 0x1F0-0x1F7. File operations follow standard FAT16 conventions: BPB parsing for disk layout, cluster chain traversal through the FAT table, and dual FAT table updates for write operations.

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
- [x] ATA PIO disk driver
- [x] FAT16 filesystem (read/write/delete/mkdir)
- [ ] Subdirectory navigation (cd)
- [ ] User program loading & execution
- [ ] Multitasking / process management

## Resources

Built following various OS development tutorials and resources, including the OSDev Wiki and heavily jumpstarted by
"Daedalus Community"s OS Dev guide. Lots of google and reddit posts too.

## License

This project is for educational purposes.