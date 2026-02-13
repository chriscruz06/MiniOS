#include "ata.h"
#include "ports.h"

// =============================================================================
// ATA PIO Driver - Talks directly to the IDE disk controller via I/O ports
//
// The primary ATA bus uses ports 0x1F0-0x1F7 and 0x3F6.
// Data transfers happen 16 bits at a time through the data port (0x1F0).
// MiniOS uses 28-bit LBA addressing which supports up to 128GB, plenty for MiniOS.
//
// Reference: OSDev Wiki "ATA PIO Mode"
// Very unfmailiar with this sort of thing, so this wiki page was a God-send.
// One of the few things I didn't have a tiny bit of knowledge about.
// =============================================================================

// -- Primary ATA bus I/O ports --
#define ATA_DATA        0x1F0   // Data register (read/write, 16-bit)
#define ATA_ERROR       0x1F1   // Error register (read)
#define ATA_FEATURES    0x1F1   // Features register (write) - same port as error
#define ATA_SECT_COUNT  0x1F2   // Number of sectors to read/write
#define ATA_LBA_LO      0x1F3   // LBA bits 0-7
#define ATA_LBA_MID     0x1F4   // LBA bits 8-15
#define ATA_LBA_HI      0x1F5   // LBA bits 16-23
#define ATA_DRIVE_HEAD  0x1F6   // Drive select + LBA bits 24-27
#define ATA_STATUS      0x1F7   // Status register (read)
#define ATA_COMMAND     0x1F7   // Command register (write) - same port as status
#define ATA_ALT_STATUS  0x3F6   // Alternate status (read, doesn't clear IRQ)
#define ATA_DEV_CTRL    0x3F6   // Device control (write)

// -- Status register bits --
#define ATA_SR_BSY      0x80    // Busy - drive is processing a command
#define ATA_SR_DRDY     0x40    // Drive ready
#define ATA_SR_DF       0x20    // Drive fault
#define ATA_SR_DSC      0x10    // Drive seek complete
#define ATA_SR_DRQ      0x08    // Data request - drive is ready for data transfer
#define ATA_SR_CORR     0x04    // Corrected data (obsolete)
#define ATA_SR_IDX      0x02    // Index (obsolete)
#define ATA_SR_ERR      0x01    // Error occurred - check error register

// -- Commands --
#define ATA_CMD_READ_PIO    0x20    // Read sectors using PIO
#define ATA_CMD_WRITE_PIO   0x30    // Write sectors using PIO
#define ATA_CMD_IDENTIFY    0xEC    // Identify drive - returns 512 bytes of info
#define ATA_CMD_FLUSH       0xE7    // Flush write cache

// -- Drive selection --
// Bit 6 = 1 for LBA mode, Bit 4 = 0 for master / 1 for slave
#define ATA_MASTER_LBA  0xE0   // 1110 0000 - master drive, LBA mode

// =============================================================================
// Internal helpers
// =============================================================================

// After selecting a drive or sending a command, the ATA spec says "you need
// to wait at least 400ns". Reading the alternate status port 4 times does this.
// Each port read takes ~100ns on a typical ISA bus.
static void ata_delay() {
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS);
}

// Poll the status register until BSY clears and DRQ sets (or error)
// Returns 0 on success, non-zero on error
static int ata_poll() {
    // Wait for BSY to clear
    ata_delay();

    // Spin while busy
    while (inb(ATA_STATUS) & ATA_SR_BSY) {
        // Intentionally empty - the drive is still processing
    }

    // Now check status
    uint8_t status = inb(ATA_STATUS);

    if (status & ATA_SR_ERR) return 1;  // Drive reported error
    if (status & ATA_SR_DF)  return 2;  // Drive fault
    if (!(status & ATA_SR_DRQ)) return 3; // DRQ not set (no data ready)

    return 0;
}

// =============================================================================
// Public API
// =============================================================================

int ata_init() {
    // Select master drive
    outb(ATA_DRIVE_HEAD, ATA_MASTER_LBA);
    ata_delay();

    // Zero out sector count and LBA ports
    outb(ATA_SECT_COUNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);

    // Send IDENTIFY command
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay();

    // Check if drive exists
    uint8_t status = inb(ATA_STATUS);
    if (status == 0) {
        // No drive on this bus
        return 1;
    }

    // Wait for BSY to clear
    while (inb(ATA_STATUS) & ATA_SR_BSY) {}

    // Check if this is actually an ATA drive (not ATAPI/SATA/etc)
    // If LBA_MID or LBA_HI become non-zero, it's not ATA
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HI) != 0) {
        return 2;  // Not an ATA drive
    }

    // Wait for DRQ or ERR
    while (1) {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) return 3;
        if (status & ATA_SR_DRQ) break;
    }

    // Read and discard the 256 words (512 bytes) of identify data
    // TODO: Could parse this for drive info (size, model string, etc)
    for (int i = 0; i < 256; i++) {
        inw(ATA_DATA);
    }

    return 0;  // Drive found and ready
}

int ata_read_sectors(uint32_t lba, uint8_t count, void* buffer) {
    if (count == 0) return 1;

    uint16_t* buf = (uint16_t*)buffer;

    // Select drive and set top 4 bits of LBA
    outb(ATA_DRIVE_HEAD, ATA_MASTER_LBA | ((lba >> 24) & 0x0F));

    // Set sector count
    outb(ATA_SECT_COUNT, count);

    // Set LBA address (low 24 bits)
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));

    // Send read command
    outb(ATA_COMMAND, ATA_CMD_READ_PIO);

    // Read each sector
    for (int s = 0; s < count; s++) {
        // Wait until data is ready
        int err = ata_poll();
        if (err) return err;

        // Read 256 words (512 bytes = 1 sector)
        // Each inw() reads 2 bytes from the data port
        for (int i = 0; i < 256; i++) {
            buf[i] = inw(ATA_DATA);
        }

        // Advance buffer pointer to next sector
        buf += 256;
    }

    return 0;
}

int ata_write_sectors(uint32_t lba, const void* buffer, uint8_t count) {
    if (count == 0) return 1;

    const uint16_t* buf = (const uint16_t*)buffer;

    // Select drive and set top 4 bits of LBA
    outb(ATA_DRIVE_HEAD, ATA_MASTER_LBA | ((lba >> 24) & 0x0F));

    // Set sector count
    outb(ATA_SECT_COUNT, count);

    // Set LBA address
    outb(ATA_LBA_LO,  (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI,  (uint8_t)((lba >> 16) & 0xFF));

    // Send write command
    outb(ATA_COMMAND, ATA_CMD_WRITE_PIO);

    // Write each sector
    for (int s = 0; s < count; s++) {
        // Wait until drive is ready for data
        int err = ata_poll();
        if (err) return err;

        // Write 256 words (512 bytes = 1 sector)
        for (int i = 0; i < 256; i++) {
            outw(ATA_DATA, buf[i]);
        }

        // Advance buffer pointer
        buf += 256;
    }

    // Flush the write cache so data actually hits the disk
    outb(ATA_COMMAND, ATA_CMD_FLUSH);

    // Wait for flush to complete
    while (inb(ATA_STATUS) & ATA_SR_BSY) {}

    return 0;
}