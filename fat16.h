#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

// FAT16 directory entry attributes
#define FAT16_ATTR_READ_ONLY  0x01
#define FAT16_ATTR_HIDDEN     0x02
#define FAT16_ATTR_SYSTEM     0x04
#define FAT16_ATTR_VOLUME_ID  0x08
#define FAT16_ATTR_DIRECTORY  0x10
#define FAT16_ATTR_ARCHIVE    0x20
#define FAT16_ATTR_LFN        0x0F   // Long filename entry (combination of all above)

// Directory entry structure (32 bytes, straight from the disk)
struct fat16_dir_entry {
    char     name[8];           // Filename (space-padded)
    char     ext[3];            // Extension (space-padded)
    uint8_t  attributes;        // File attributes
    uint8_t  reserved;          // Reserved for Windows NT
    uint8_t  create_time_ms;    // Creation time (tenths of a second)
    uint16_t create_time;       // Creation time
    uint16_t create_date;       // Creation date
    uint16_t access_date;       // Last access date
    uint16_t cluster_high;      // High 16 bits of cluster (always 0 for FAT16)
    uint16_t modify_time;       // Last modification time
    uint16_t modify_date;       // Last modification date
    uint16_t cluster_low;       // Starting cluster number
    uint32_t file_size;         // File size in bytes
} __attribute__((packed));

// Initialize FAT16 driver - reads BPB and calculates layout
// Returns 0 on success
int fat16_init();

// List files in root directory
// Prints filename, size, and attributes for each entry
void fat16_list_root();

// Read a file from root directory into buffer
// filename: "README  TXT" format (8.3, space padded) OR "README.TXT" format
// buffer: destination (must be large enough for file)
// max_size: maximum bytes to read
// Returns bytes read, or -1 on error
int fat16_read_file(const char* filename, void* buffer, uint32_t max_size);

// Get the size of a file in the root directory
// Returns file size, or -1 if not found
int fat16_file_size(const char* filename);

// Create or overwrite a file in the root directory
// If file exists, it will be deleted first
// data can be nullptr for empty files (touch)
// Returns 0 on success, non-zero on error
int fat16_create_file(const char* filename, const void* data, uint32_t size);

// Delete a file or empty directory from the root directory
// Frees all clusters in the chain and marks entry as deleted
// Returns 0 on success, -1 if not found
int fat16_delete(const char* filename);

// Create a subdirectory in the root directory
// Allocates a cluster and initializes . and .. entries
// Returns 0 on success, non-zero on error
int fat16_mkdir(const char* dirname);

#endif