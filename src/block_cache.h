#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H

#include "util.h"
#include "block_writer.h"

// The segment size defines the minimum read/write size
// actually made to the output. Additionally, all reads and
// writes will be aligned to that size.
#define BLOCK_CACHE_SEGMENT_SIZE       (128*1024) // This is also the read/write write size
#define BLOCK_CACHE_NUM_SEGMENTS       64         // 8 MB cache
#define BLOCK_CACHE_BLOCKS_PER_SEGMENT (BLOCK_CACHE_SEGMENT_SIZE / BLOCK_SIZE)
#define BLOCK_CACHE_SEGMENT_MASK       (~(BLOCK_CACHE_SEGMENT_SIZE - 1))

struct block_cache_segment {
    bool in_use;

    // The data in this segment.
    uint8_t *data;

    // Where this segment is located
    off_t offset;

    // "timestamp" for computing least recently used
    uint32_t last_access;

    // Set to true if all of the data written to this segment
    // has been streamed. If true and the entire segment is marked
    // dirty, then it should be written to the target asap so that
    // the actual writes can start.
    bool streamed;

    // True if an asynchronous write is in progress
    bool write_in_progress;

    // Bit fields for determining whether blocks inside the
    // segment are valid (hold the most up-to-date data) and/or
    // dirty (need to be written back to the target image).
    // (2 bits of flags in a uint8_t)
    uint8_t flags[BLOCK_CACHE_BLOCKS_PER_SEGMENT * 2 / 8];
};

struct block_cache {
    struct block_writer writer;

    uint32_t timestamp;

    int fd;

    struct block_cache_segment segments[BLOCK_CACHE_NUM_SEGMENTS];

    // Temporary buffer for reading segments that are partially valid
    uint8_t *temp;

    // Track "trimmed" segments in a bitfield. One bit per segment.
    // E.g., 128K/bit -> 1M takes represented in 1 byte -> 1G in 1 KB, etc.
    size_t trimmed_len;
    uint8_t *trimmed;
    bool trimmed_remainder; // true if segments after end of bitfield are trimmed
};

int block_cache_init(struct block_cache *bc, int fd);
int block_cache_trim(struct block_cache *bc, off_t offset, off_t count);
int block_cache_trim_after(struct block_cache *bc, off_t offset);
int block_cache_pwrite(struct block_cache *bc, const void *buf, size_t count, off_t offset, bool streamed);
int block_cache_pread(struct block_cache *bc, void *buf, size_t count, off_t offset);
int block_cache_flush(struct block_cache *bc);
int block_cache_free(struct block_cache *bc);

#endif // BLOCK_CACHE_H
