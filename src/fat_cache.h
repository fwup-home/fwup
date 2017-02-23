#ifndef FAT_CACHE_H
#define FAT_CACHE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

struct fat_cache {
    int fd;
    off_t partition_offset;

    char *cache;
    size_t cache_size_blocks;

    // Bit field of invalid and dirty bits
    uint8_t *flags;

    // read_on_invalid is true if the SDCard should
    // be read when accessing an invalid block in the cache. If false,
    // the block is filled with zeros.
    bool read_on_invalid;
};

int fat_cache_init(struct fat_cache *fc, int fd, off_t partition_offset, size_t cache_size);
void fat_cache_format(struct fat_cache *fc);
int fat_cache_read(struct fat_cache *fc, off_t block, size_t count, char *buffer);
ssize_t fat_cache_write(struct fat_cache *fc, off_t block, size_t count, const char *buffer);
ssize_t fat_cache_free(struct fat_cache *fc);

#endif // FAT_CACHE_H
