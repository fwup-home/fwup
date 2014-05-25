#ifndef MBR_H
#define MBR_H

struct mbr_partition {
    int boot_flag;      // 1 to mark as boot partition
    int partition_type; // partition type (e.g., 0x83 for Linux, 0x01 for FAT12, 0x04 for FAT16, 0x0c for FAT32, etc.
    int block_offset;
    int block_count;
};

int mbr_create(struct mbr_partition partitions[4], const char *bootstrap, char output[512]);
const char *mbr_last_error();

#endif // MBR_H
