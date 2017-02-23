#include "fat_cache.h"
#include "util.h"

#include <memory.h>
#include <unistd.h>

// Cache bit handling functions
static inline void set_dirty(struct fat_cache *fc, int block)
{
    // Dirty implies that the block is valid, so make sure that it's set too.
    uint8_t *bits = &fc->flags[block / 4];
    *bits = *bits | (0x3 << (2 * (block & 0x3)));
}

static inline bool is_dirty(struct fat_cache *fc, int block)
{
    return (fc->flags[block / 4] & (0x1 << (2 * (block & 0x3)))) != 0;
}

static inline void set_valid(struct fat_cache *fc, int block)
{
    uint8_t *bits = &fc->flags[block / 4];
    *bits = *bits | (0x2 << (2 * (block & 0x3)));
}

static inline bool is_valid(struct fat_cache *fc, int block)
{
    return (fc->flags[block / 4] & (0x2 << (2 * (block & 0x3)))) != 0;
}

/**
 * @brief Simple cache for operating on FAT filesystems
 *
 * The FATFS code makes small 512 byte reads and writes to the underlying media. On systems
 * that don't have a cache between fwup and the disk, this can double the update time. Caching
 * is fairly simple due to some simple assumptions:
 *
 *   1. We only cache the first n bytes of the filesystem, since fwup isn't really intended
 *      to handle large FAT filesystems in regular operation. Reads and writes after this
 *      aren't cached.
 *   2. The FAT table at the beginning is the big abuser of small and repeated reads and
 *      writes.
 *   3. Writes are mostly contiguous. This means that combining FATFS writes into larger
 *      blocks will be a win. If the FS is heavily fragmented, this won't help much, but
 *      the boot filesystems modified by fwup really shouldn't become heavily fragmented.
 *
 * @param fc
 * @param fd              the target file to write to
 * @param cache_size      the size of the cache (e.g. the size of the filesystem in bytes or some fraction)
 * @return 0 if ok, -1 if not
 */
int fat_cache_init(struct fat_cache *fc, int fd, off_t partition_offset, size_t cache_size)
{
    fc->fd = fd;
    fc->partition_offset = partition_offset;

    fc->cache = malloc(cache_size);
    if (!fc->cache)
        ERR_RETURN("Could not allocate FAT cache of %d bytes", cache_size);
    fc->cache_size_blocks = cache_size / 512;

    fc->flags = malloc(fc->cache_size_blocks / 4);
    if (!fc->flags)
        ERR_RETURN("Could not allocate FAT cache flags");
    memset(fc->flags, 0, fc->cache_size_blocks / 4);

    fc->read_on_invalid = true;
    return 0;
}

/**
 * @brief "Erase" everything in the partition
 *
 * This really just invalidates everything in the cache and sets it so
 * that any reads will return zero'd blocks.
 *
 * @param fc
 * @return
 */
void fat_cache_format(struct fat_cache *fc)
{
    fc->read_on_invalid = false;
    memset(fc->flags, 0, fc->cache_size_blocks / 4);

    // Optimization: Initialize the first 128 KB or else there's a
    // random patchwork in the beginning that gets flushed at the end.
    if (fc->cache_size_blocks >= 256) {
        memset(fc->cache, 0, 256 * 512);
        memset(fc->flags, 0xff, 256 / 4);
    }
}

static ssize_t load_cache(struct fat_cache *fc, int block, int count)
{
    size_t byte_count = count * 512;
    ssize_t rc = 0;
    if (fc->read_on_invalid) {
        off_t byte_offset = fc->partition_offset + block * 512;
        rc = pread(fc->fd, &fc->cache[block * 512], byte_count, byte_offset);
        if (rc < 0)
            ERR_RETURN("Error reading FAT filesystem");
    } else {
        // Not sure why we're reading an uninitialized block, but set it to 0s
        memset(&fc->cache[block * 512], 0, byte_count);
    }

    int i;
    for (i = 0; i < count; i++)
        set_valid(fc, block + i);

    return rc;
}

/**
 * @brief Read from the cache
 *
 * @param fc
 * @param block
 * @param count
 * @param buffer
 * @return
 */
int fat_cache_read(struct fat_cache *fc, off_t block, size_t count, char *buffer)
{
    // Fetch anything directly that's beyond what we cache
    if (block + count > fc->cache_size_blocks) {
        off_t uncached_block = (block > (off_t) fc->cache_size_blocks ? block : (off_t) fc->cache_size_blocks);
        off_t uncached_count = block + count - uncached_block;
        char *uncached_buffer = buffer + (uncached_block - block) * 512;

        off_t byte_offset = fc->partition_offset + uncached_block * 512;
        size_t byte_count = uncached_count * 512;

        ssize_t amount_read = pread(fc->fd, uncached_buffer, byte_count, byte_offset);
        if (amount_read < 0)
            ERR_RETURN("Can't read block %d, count=%d", uncached_block, uncached_count);

        // Adjust the count so that we're just left with the part in the cached area
        count -= uncached_count;
        if (count == 0)
            return 0;
    }

    // The common case is that the block will either be all valid or all invalid
    bool all_valid = true;
    bool all_invalid = true;
    size_t i;
    for (i = 0; i < count; i++) {
        bool v = is_valid(fc, block + i);
        all_valid = all_valid && v;
        all_invalid = all_invalid && !v;
    }

    if (all_invalid) {
        // If we have to read from Flash, see if we can read up to 128 KB (256 * 512 byte blocks)
        size_t precache_count = count;
        for (; precache_count < 256 && block + precache_count < fc->cache_size_blocks; precache_count++) {
            if (is_valid(fc, block + precache_count))
                break;
        }
        if (load_cache(fc, block, precache_count) < 0)
            return -1;
    } else {
        if (!all_valid) {
            // Not all invalid and not all valid. Punt. Just load each block individually, since this is
            // a rare case.
            for (i = 0; i < count; i++) {
                if (!is_valid(fc, block + i) && load_cache(fc, block + i, 1) < 0)
                    return -1;
            }

        }
    }

    memcpy(buffer, &fc->cache[block * 512], count * 512);
    return 0;
}

/**
 * @brief Write to the cache
 *
 * If the write crosses over the area that we cache, then write directly to the output
 * without buffering.
 *
 * @param fc
 * @param block     the starting block
 * @param count     how many blocks to write
 * @param buffer    the data to write
 * @return the number of bytes written to disk or -1 if error. Normally this is
 *         0 to indicate that everything was written to cache
 */
ssize_t fat_cache_write(struct fat_cache *fc, off_t block, size_t count, const char *buffer)
{
    ssize_t rc = 0;
    off_t last = block + count;
    for (; block < last && block < (off_t) fc->cache_size_blocks; block++) {
        memcpy(&fc->cache[512 * block], buffer, 512);
        set_dirty(fc, block);
        buffer += 512;
    }
    if (block != last) {
        off_t byte_offset = fc->partition_offset + block * 512;
        size_t byte_count = (last - block) * 512;
        rc = pwrite(fc->fd, buffer, byte_count, byte_offset);
    }
    return rc;
}

static ssize_t flush_buffer(struct fat_cache *fc, int block, int count)
{
    off_t byte_offset = fc->partition_offset + block * 512;
    int cache_index = block * 512;
    size_t byte_count = count * 512;
    ssize_t amount_written = 0;
    while (byte_count) {
        // Write only 128 KB at a time, since raw writing too much
        // may not be handled well by the OS.
        size_t to_write = 128 * 1024;
        if (to_write > byte_count)
            to_write = byte_count;

        ssize_t rc = pwrite(fc->fd, &fc->cache[cache_index], to_write, byte_offset);
        if (rc < 0)
            ERR_RETURN("Error writing FAT filesystem");

        byte_offset += to_write;
        cache_index += to_write;
        byte_count -= to_write;
        amount_written += rc;
    }
    return amount_written;
}

/**
 * @brief Flush the FAT cache and free memory
 *
 * @param fc
 * @return the number of bytes written to the device or -1 if error
 */
ssize_t fat_cache_free(struct fat_cache *fc)
{
    ssize_t amount_written = 0;
    int starting_block = -1;
    off_t block;
    for (block = 0; block < (off_t) fc->cache_size_blocks; block++) {
        if (is_dirty(fc, block)) {
            if (starting_block < 0)
                starting_block = block;
        } else {
            if (starting_block >= 0) {
                ssize_t rc = flush_buffer(fc, starting_block, block - starting_block);
                if (rc < 0)
                    return rc;
                amount_written += rc;
                starting_block = -1;
            }
        }
    }
    if (starting_block >= 0) {
        ssize_t rc = flush_buffer(fc, starting_block, block - starting_block);
        if (rc < 0)
            return rc;
        amount_written += rc;
    }

    free(fc->flags);
    free(fc->cache);

    fc->cache_size_blocks = 0;

    return amount_written;
}
