#include "fat16.h"
#include "ata.h"
#include "vga.h"

// =============================================================================
// FAT16 Filesystem Driver
//
// FAT16 disk layout (in order):
//   1. Boot sector / BPB     (sector 0)
//   2. Reserved sectors      (sectors 1 to reserved_sectors-1)
//   3. FAT table #1          (fat_size_16 sectors)
//   4. FAT table #2          (fat_size_16 sectors, copy of #1)
//   5. Root directory         (fixed size, right after FATs)
//   6. Data region           (where actual file/folder data lives)
//
// Cluster numbering starts at 2 (clusters 0 and 1 are reserved).
// A cluster is just a group of consecutive sectors in the data region.
//
// To read a file:
//   1. Find the file's directory entry in the root directory
//   2. Get the starting cluster number from the entry
//   3. Read that cluster's data from the data region
//   4. Look up the next cluster in the FAT table
//   5. Repeat until FAT entry >= 0xFFF8 (end of chain)
// =============================================================================

// -- BPB fields we care about (parsed from sector 0) --
static uint16_t bytes_per_sector;
static uint8_t  sectors_per_cluster;
static uint16_t reserved_sectors;
static uint8_t  num_fats;
static uint16_t root_entry_count;
static uint16_t total_sectors;
static uint16_t fat_size_16;        // Sectors per FAT

// -- Calculated layout values --
static uint32_t fat_start_lba;       // Where FAT table #1 begins
static uint32_t root_dir_start_lba;  // Where root directory begins
static uint32_t root_dir_sectors;    // How many sectors the root dir occupies
static uint32_t data_start_lba;      // Where the data region begins (cluster 2)

static bool initialized = false;

// Sector buffer - reused for disk reads
static uint8_t sector_buf[512];

// Second buffer for write operations (avoids clobbering sector_buf mid-operation)
static uint8_t write_buf[512];

// =============================================================================
// Internal helpers
// =============================================================================

// Convert a cluster number to its LBA (sector) address on disk
static uint32_t cluster_to_lba(uint16_t cluster) {
    return data_start_lba + ((uint32_t)(cluster - 2) * sectors_per_cluster);
}

// Look up the next cluster in the FAT chain
// Returns the FAT entry for 'cluster' (next cluster, or >= 0xFFF8 for end)
static uint16_t fat_next_cluster(uint16_t cluster) {
    // Each FAT16 entry is 2 bytes, so 256 entries per 512-byte sector
    // Figure out which sector of the FAT contains this entry
    uint32_t fat_offset = (uint32_t)cluster * 2;
    uint32_t fat_sector = fat_start_lba + (fat_offset / bytes_per_sector);
    uint32_t entry_offset = fat_offset % bytes_per_sector;
    
    // Read that FAT sector
    if (ata_read_sectors(fat_sector, 1, sector_buf) != 0) {
        return 0xFFFF;  // Error - treat as end of chain
    }
    
    // Read the 16-bit entry (little-endian)
    uint16_t next = sector_buf[entry_offset] | (sector_buf[entry_offset + 1] << 8);
    return next;
}

// Compare a user-provided filename against a directory entry
// Handles both "README.TXT" and "README  TXT" formats
static bool fat16_name_match(const fat16_dir_entry* entry, const char* filename) {
    char fat_name[12];  // 8.3 format: 8 name + 3 ext (no dot stored)
    
    // Build the FAT-format name from user input
    int i = 0, j = 0;
    
    // Copy name part (up to 8 chars, pad with spaces)
    while (filename[i] && filename[i] != '.' && j < 8) {
        // Convert to uppercase for comparison
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        fat_name[j++] = c;
        i++;
    }
    while (j < 8) fat_name[j++] = ' ';
    
    // Skip the dot if present
    if (filename[i] == '.') i++;
    
    // Copy extension (up to 3 chars, pad with spaces)
    while (filename[i] && j < 11) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        fat_name[j++] = c;
        i++;
    }
    while (j < 11) fat_name[j++] = ' ';
    fat_name[11] = '\0';
    
    // Compare against the directory entry's name
    for (int k = 0; k < 11; k++) {
        if (entry->name[k] != fat_name[k]) return false;
    }
    return true;
}

// Find a file in the root directory by name
// Returns the directory entry, or nullptr if not found
static fat16_dir_entry* fat16_find_in_root(const char* filename) {
    static fat16_dir_entry found_entry;  // Static so we can return a pointer
    
    for (uint32_t sec = 0; sec < root_dir_sectors; sec++) {
        if (ata_read_sectors(root_dir_start_lba + sec, 1, sector_buf) != 0) {
            return nullptr;
        }
        
        fat16_dir_entry* entries = (fat16_dir_entry*)sector_buf;
        int entries_per_sector = bytes_per_sector / sizeof(fat16_dir_entry);
        
        for (int i = 0; i < entries_per_sector; i++) {
            // First byte 0x00 = no more entries
            if (entries[i].name[0] == 0x00) return nullptr;
            // First byte 0xE5 = deleted entry, skip
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            // Skip long filename entries and volume labels
            if (entries[i].attributes & FAT16_ATTR_LFN) continue;
            if (entries[i].attributes & FAT16_ATTR_VOLUME_ID) continue;
            
            if (fat16_name_match(&entries[i], filename)) {
                // Copy it out since sector_buf will be overwritten
                uint8_t* dst = (uint8_t*)&found_entry;
                uint8_t* src = (uint8_t*)&entries[i];
                for (uint32_t b = 0; b < sizeof(fat16_dir_entry); b++) {
                    dst[b] = src[b];
                }
                return &found_entry;
            }
        }
    }
    
    return nullptr;
}

// Convert a user-friendly filename to FAT 8.3 format (11 bytes, no dot, space-padded)
// e.g. "hello.txt" -> "HELLO   TXT"
static void fat16_make_83_name(const char* filename, char* fat_name) {
    int i = 0, j = 0;
    
    // Copy name part (up to 8 chars, pad with spaces, uppercase)
    while (filename[i] && filename[i] != '.' && j < 8) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        fat_name[j++] = c;
        i++;
    }
    while (j < 8) fat_name[j++] = ' ';
    
    // Skip the dot
    if (filename[i] == '.') i++;
    
    // Copy extension (up to 3 chars, pad with spaces)
    while (filename[i] && j < 11) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        fat_name[j++] = c;
        i++;
    }
    while (j < 11) fat_name[j++] = ' ';
}

// Write a 16-bit value to a FAT entry for a given cluster
// Updates both FAT copies for consistency
static int fat16_write_fat_entry(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset = (uint32_t)cluster * 2;
    uint32_t fat_sector = fat_start_lba + (fat_offset / bytes_per_sector);
    uint32_t entry_offset = fat_offset % bytes_per_sector;
    
    // Read the FAT sector into write_buf (not sector_buf, to avoid clobbering)
    if (ata_read_sectors(fat_sector, 1, write_buf) != 0) return -1;
    
    // Modify the entry (little-endian)
    write_buf[entry_offset] = value & 0xFF;
    write_buf[entry_offset + 1] = (value >> 8) & 0xFF;
    
    // Write back to FAT #1
    if (ata_write_sectors(fat_sector, write_buf, 1) != 0) return -1;
    
    // Write back to FAT #2 (mirror copy)
    uint32_t fat2_sector = fat_sector + fat_size_16;
    if (ata_write_sectors(fat2_sector, write_buf, 1) != 0) return -1;
    
    return 0;
}

// Find a free cluster in the FAT (entry value == 0x0000)
// Returns cluster number, or -1 if disk is full
static int fat16_alloc_cluster() {
    uint32_t total_data_sectors = total_sectors - data_start_lba;
    uint32_t total_clusters = total_data_sectors / sectors_per_cluster;
    
    // Scan from cluster 2 (0 and 1 are reserved)
    for (uint16_t cluster = 2; cluster < total_clusters + 2; cluster++) {
        uint16_t val = fat_next_cluster(cluster);
        if (val == 0x0000) {
            return (int)cluster;
        }
    }
    return -1;  // Disk full
}

// Free an entire cluster chain starting at 'cluster'
// Walks the chain and marks each entry as 0x0000 (free)
static void fat16_free_chain(uint16_t cluster) {
    while (cluster >= 2 && cluster < 0xFFF8) {
        uint16_t next = fat_next_cluster(cluster);  // Save next before we overwrite
        fat16_write_fat_entry(cluster, 0x0000);
        cluster = next;
    }
}

// =============================================================================
// Public API
// =============================================================================

int fat16_init() {
    // Read the boot sector / BPB
    if (ata_read_sectors(0, 1, sector_buf) != 0) {
        return 1;
    }
    
    // Check boot signature
    if (sector_buf[510] != 0x55 || sector_buf[511] != 0xAA) {
        return 2;
    }
    
    // Parse BPB fields
    bytes_per_sector    = sector_buf[11] | (sector_buf[12] << 8);
    sectors_per_cluster = sector_buf[13];
    reserved_sectors    = sector_buf[14] | (sector_buf[15] << 8);
    num_fats            = sector_buf[16];
    root_entry_count    = sector_buf[17] | (sector_buf[18] << 8);
    total_sectors       = sector_buf[19] | (sector_buf[20] << 8);
    fat_size_16         = sector_buf[22] | (sector_buf[23] << 8);
    
    // Sanity checks
    if (bytes_per_sector != 512) return 3;
    if (sectors_per_cluster == 0) return 4;
    if (num_fats == 0) return 5;
    
    // Calculate layout
    fat_start_lba      = reserved_sectors;
    root_dir_start_lba = reserved_sectors + ((uint32_t)num_fats * fat_size_16);
    root_dir_sectors   = ((uint32_t)root_entry_count * 32 + bytes_per_sector - 1) / bytes_per_sector;
    data_start_lba     = root_dir_start_lba + root_dir_sectors;
    
    initialized = true;
    return 0;
}

void fat16_list_root() {
    if (!initialized) {
        vga_print("FAT16 not initialized\n");
        return;
    }
    
    int file_count = 0;
    uint32_t total_size = 0;
    
    for (uint32_t sec = 0; sec < root_dir_sectors; sec++) {
        if (ata_read_sectors(root_dir_start_lba + sec, 1, sector_buf) != 0) {
            vga_print("Error reading root directory\n");
            return;
        }
        
        fat16_dir_entry* entries = (fat16_dir_entry*)sector_buf;
        int entries_per_sector = bytes_per_sector / sizeof(fat16_dir_entry);
        
        for (int i = 0; i < entries_per_sector; i++) {
            // End of directory
            if (entries[i].name[0] == 0x00) goto done;
            // Deleted entry
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            // Skip LFN entries
            if (entries[i].attributes == FAT16_ATTR_LFN) continue;
            // Skip volume label
            if (entries[i].attributes & FAT16_ATTR_VOLUME_ID) continue;
            
            // Print attributes indicator
            if (entries[i].attributes & FAT16_ATTR_DIRECTORY) {
                vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
                vga_print("  <DIR>  ");
            } else {
                vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
                vga_print("  ");
                // Right-align file size (pad to 7 chars)
                uint32_t sz = entries[i].file_size;
                int digits = 0;
                uint32_t tmp = sz;
                do { digits++; tmp /= 10; } while (tmp > 0);
                for (int p = digits; p < 7; p++) vga_put_char(' ');
                vga_print_int(sz);
                vga_print("  ");
            }
            
            // Print filename
            vga_set_color(VGA_WHITE, VGA_BLACK);
            for (int n = 0; n < 8; n++) {
                if (entries[i].name[n] != ' ') {
                    // Print lowercase for readability
                    char c = entries[i].name[n];
                    if (c >= 'A' && c <= 'Z') c += 32;
                    vga_put_char(c);
                }
            }
            
            // Print extension with dot
            if (entries[i].ext[0] != ' ') {
                vga_put_char('.');
                for (int e = 0; e < 3; e++) {
                    if (entries[i].ext[e] != ' ') {
                        char c = entries[i].ext[e];
                        if (c >= 'A' && c <= 'Z') c += 32;
                        vga_put_char(c);
                    }
                }
            }
            
            vga_put_char('\n');
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            
            if (!(entries[i].attributes & FAT16_ATTR_DIRECTORY)) {
                total_size += entries[i].file_size;
            }
            file_count++;
        }
    }
    
done:
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_print("  ");
    vga_print_int(file_count);
    vga_print(" file(s), ");
    vga_print_int(total_size);
    vga_print(" bytes total\n");
}

int fat16_read_file(const char* filename, void* buffer, uint32_t max_size) {
    if (!initialized) return -1;
    
    // Find the file
    fat16_dir_entry* entry = fat16_find_in_root(filename);
    if (!entry) return -1;
    
    // Don't try to read directories this way
    if (entry->attributes & FAT16_ATTR_DIRECTORY) return -1;
    
    uint32_t file_size = entry->file_size;
    uint32_t to_read = file_size;
    if (to_read > max_size) to_read = max_size;
    
    uint16_t cluster = entry->cluster_low;
    uint32_t bytes_read = 0;
    uint8_t* buf = (uint8_t*)buffer;
    uint32_t cluster_size = (uint32_t)sectors_per_cluster * bytes_per_sector;
    
    while (bytes_read < to_read) {
        // Validate cluster number
        if (cluster < 2 || cluster >= 0xFFF8) break;
        
        // Read all sectors in this cluster
        uint32_t lba = cluster_to_lba(cluster);
        
        for (uint8_t s = 0; s < sectors_per_cluster && bytes_read < to_read; s++) {
            if (ata_read_sectors(lba + s, 1, sector_buf) != 0) {
                return -1;  // Read error
            }
            
            // Copy data from sector to output buffer
            uint32_t remaining = to_read - bytes_read;
            uint32_t copy_size = (remaining < bytes_per_sector) ? remaining : bytes_per_sector;
            
            for (uint32_t b = 0; b < copy_size; b++) {
                buf[bytes_read + b] = sector_buf[b];
            }
            bytes_read += copy_size;
        }
        
        // Follow the chain to the next cluster
        cluster = fat_next_cluster(cluster);
    }
    
    return (int)bytes_read;
}

int fat16_file_size(const char* filename) {
    if (!initialized) return -1;
    
    fat16_dir_entry* entry = fat16_find_in_root(filename);
    if (!entry) return -1;
    
    return (int)entry->file_size;
}

int fat16_create_file(const char* filename, const void* data, uint32_t size) {
    if (!initialized) return -1;
    
    // Delete existing file first if it exists (overwrite behavior)
    fat16_delete(filename);
    
    // Find a free root directory entry
    int dir_sector = -1;
    int dir_index = -1;
    
    for (uint32_t sec = 0; sec < root_dir_sectors; sec++) {
        if (ata_read_sectors(root_dir_start_lba + sec, 1, sector_buf) != 0) return -1;
        
        fat16_dir_entry* entries = (fat16_dir_entry*)sector_buf;
        int entries_per_sector = bytes_per_sector / sizeof(fat16_dir_entry);
        
        for (int i = 0; i < entries_per_sector; i++) {
            if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                dir_sector = sec;
                dir_index = i;
                goto found_slot;
            }
        }
    }
    return -1;  // Root directory full

found_slot:
    uint16_t first_cluster = 0;
    
    // If there's data, allocate clusters and write it
    if (size > 0 && data != nullptr) {
        uint32_t cluster_size = (uint32_t)sectors_per_cluster * bytes_per_sector;
        uint32_t clusters_needed = (size + cluster_size - 1) / cluster_size;
        
        uint16_t prev_cluster = 0;
        const uint8_t* src = (const uint8_t*)data;
        uint32_t bytes_written = 0;
        
        for (uint32_t c = 0; c < clusters_needed; c++) {
            // Find a free cluster
            int cluster = fat16_alloc_cluster();
            if (cluster < 0) return -1;  // Disk full
            
            // Remember the first cluster for the directory entry
            if (c == 0) first_cluster = (uint16_t)cluster;
            
            // Chain: previous cluster points to this one
            if (prev_cluster != 0) {
                if (fat16_write_fat_entry(prev_cluster, (uint16_t)cluster) != 0) return -1;
            }
            
            // Mark this cluster as end-of-chain (updated if more clusters follow)
            if (fat16_write_fat_entry((uint16_t)cluster, 0xFFFF) != 0) return -1;
            
            // Write data into this cluster's sectors
            uint32_t lba = cluster_to_lba((uint16_t)cluster);
            for (uint8_t s = 0; s < sectors_per_cluster && bytes_written < size; s++) {
                // Clear buffer, then copy data in
                for (int b = 0; b < 512; b++) write_buf[b] = 0;
                
                uint32_t remaining = size - bytes_written;
                uint32_t copy_size = (remaining < 512) ? remaining : 512;
                for (uint32_t b = 0; b < copy_size; b++) {
                    write_buf[b] = src[bytes_written + b];
                }
                
                if (ata_write_sectors(lba + s, write_buf, 1) != 0) return -1;
                bytes_written += copy_size;
            }
            
            prev_cluster = (uint16_t)cluster;
        }
    }
    
    // Now create the directory entry
    // Re-read the directory sector (may have been clobbered by FAT operations above)
    if (ata_read_sectors(root_dir_start_lba + dir_sector, 1, sector_buf) != 0) return -1;
    
    fat16_dir_entry* entries = (fat16_dir_entry*)sector_buf;
    fat16_dir_entry* entry = &entries[dir_index];
    
    // Clear the entire 32-byte entry
    uint8_t* ep = (uint8_t*)entry;
    for (int b = 0; b < 32; b++) ep[b] = 0;
    
    // Set 8.3 filename (writes into name[8] + ext[3] which are contiguous)
    fat16_make_83_name(filename, entry->name);
    
    entry->attributes = FAT16_ATTR_ARCHIVE;
    entry->cluster_low = first_cluster;
    entry->file_size = size;
    
    // Write directory sector back to disk
    if (ata_write_sectors(root_dir_start_lba + dir_sector, sector_buf, 1) != 0) return -1;
    
    return 0;
}

int fat16_delete(const char* filename) {
    if (!initialized) return -1;
    
    for (uint32_t sec = 0; sec < root_dir_sectors; sec++) {
        if (ata_read_sectors(root_dir_start_lba + sec, 1, sector_buf) != 0) return -1;
        
        fat16_dir_entry* entries = (fat16_dir_entry*)sector_buf;
        int entries_per_sector = bytes_per_sector / sizeof(fat16_dir_entry);
        
        for (int i = 0; i < entries_per_sector; i++) {
            // End of directory
            if (entries[i].name[0] == 0x00) return -1;
            // Skip deleted/LFN/volume entries
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attributes == FAT16_ATTR_LFN) continue;
            if (entries[i].attributes & FAT16_ATTR_VOLUME_ID) continue;
            
            if (fat16_name_match(&entries[i], filename)) {
                // Save cluster before freeing (free_chain clobbers sector_buf)
                uint16_t cluster = entries[i].cluster_low;
                
                // Free the cluster chain
                if (cluster >= 2) {
                    fat16_free_chain(cluster);
                }
                
                // Re-read directory sector since free_chain clobbered sector_buf
                if (ata_read_sectors(root_dir_start_lba + sec, 1, sector_buf) != 0) return -1;
                
                // Mark entry as deleted
                fat16_dir_entry* re_entries = (fat16_dir_entry*)sector_buf;
                re_entries[i].name[0] = (char)0xE5;
                
                // Write it back
                if (ata_write_sectors(root_dir_start_lba + sec, sector_buf, 1) != 0) return -1;
                
                return 0;
            }
        }
    }
    
    return -1;  // Not found
}

int fat16_mkdir(const char* dirname) {
    if (!initialized) return -1;
    
    // Check if it already exists
    fat16_dir_entry* existing = fat16_find_in_root(dirname);
    if (existing) return -1;  // Name already taken
    
    // Allocate one cluster for the new directory's contents
    int cluster = fat16_alloc_cluster();
    if (cluster < 0) return -1;  // Disk full
    
    // Mark as end of chain
    if (fat16_write_fat_entry((uint16_t)cluster, 0xFFFF) != 0) return -1;
    
    // Initialize the directory cluster: clear all sectors, then write . and ..
    uint32_t lba = cluster_to_lba((uint16_t)cluster);
    
    for (uint8_t s = 0; s < sectors_per_cluster; s++) {
        // Clear buffer
        for (int b = 0; b < 512; b++) write_buf[b] = 0;
        
        // First sector gets the . and .. entries
        if (s == 0) {
            fat16_dir_entry* dot = (fat16_dir_entry*)write_buf;
            
            // "." entry - points to itself
            dot[0].name[0] = '.';
            for (int j = 1; j < 8; j++) dot[0].name[j] = ' ';
            for (int j = 0; j < 3; j++) dot[0].ext[j] = ' ';
            dot[0].attributes = FAT16_ATTR_DIRECTORY;
            dot[0].cluster_low = (uint16_t)cluster;
            dot[0].file_size = 0;
            
            // ".." entry - points to root (cluster 0 means root in FAT16)
            dot[1].name[0] = '.';
            dot[1].name[1] = '.';
            for (int j = 2; j < 8; j++) dot[1].name[j] = ' ';
            for (int j = 0; j < 3; j++) dot[1].ext[j] = ' ';
            dot[1].attributes = FAT16_ATTR_DIRECTORY;
            dot[1].cluster_low = 0;
            dot[1].file_size = 0;
        }
        
        if (ata_write_sectors(lba + s, write_buf, 1) != 0) {
            fat16_write_fat_entry((uint16_t)cluster, 0x0000);  // Cleanup on failure
            return -1;
        }
    }
    
    // Find a free root directory entry and write it
    for (uint32_t sec = 0; sec < root_dir_sectors; sec++) {
        if (ata_read_sectors(root_dir_start_lba + sec, 1, sector_buf) != 0) {
            fat16_free_chain((uint16_t)cluster);
            return -1;
        }
        
        fat16_dir_entry* entries = (fat16_dir_entry*)sector_buf;
        int entries_per_sector = bytes_per_sector / sizeof(fat16_dir_entry);
        
        for (int i = 0; i < entries_per_sector; i++) {
            if (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
                // Clear entry
                uint8_t* ep = (uint8_t*)&entries[i];
                for (int b = 0; b < 32; b++) ep[b] = 0;
                
                // Set directory name and attributes
                fat16_make_83_name(dirname, entries[i].name);
                entries[i].attributes = FAT16_ATTR_DIRECTORY;
                entries[i].cluster_low = (uint16_t)cluster;
                entries[i].file_size = 0;  // Directories always have size 0 in FAT16
                
                // Write back
                if (ata_write_sectors(root_dir_start_lba + sec, sector_buf, 1) != 0) {
                    fat16_free_chain((uint16_t)cluster);
                    return -1;
                }
                
                return 0;
            }
        }
    }
    
    // No free directory entry found - clean up the allocated cluster
    fat16_free_chain((uint16_t)cluster);
    return -1;
}