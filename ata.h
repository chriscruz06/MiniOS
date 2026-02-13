#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_SECTOR_SIZE 512

// Initialize ATA driver, detect primary drive
// Returns 0 on success, nonzero if no drive found
int ata_init();

// Read 'count' sectors starting at LBA into buffer
// buffer must be at least (count * 512) bytes
// Returns 0 on success, non-zero on error
int ata_read_sectors(uint32_t lba, uint8_t count, void* buffer);

// Write 'count' sectors starting at LBA from buffer
// Returns 0 on success, non-zero on error
int ata_write_sectors(uint32_t lba, const void* buffer, uint8_t count);

#endif