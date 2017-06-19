#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H

#include "util.h"
#include "block_writer.h"

#define BLOCK_CACHE_MAX_SEGMENT_SIZE  (256*1024)

struct block_cache_segment {
    struct block_cache_segment *next;

    // Where the segment is located
    off_t offset;

    // "timestamp" for computing least recently used
    uint32_t last_access;

    // Set to true if all of the data written to this segment
    // has been streamed. If true and the entire segment is marked
    // dirty, then it should be written to the target asap so that
    // the actual writes can start.
    bool streamed;

    // Bit fields for determining whether blocks inside the
    // segment are valid (match what's on the target image) or
    // dirty (need to be written back to the target image).
    uint8_t valid[BLOCK_CACHE_MAX_SEGMENT_SIZE / BLOCK_SIZE];
    uint8_t dirty[BLOCK_CACHE_MAX_SEGMENT_SIZE / BLOCK_SIZE];

    // The data in this segment.
    uint8_t data[0];
};

struct block_cache {
    struct block_writer writer;

    uint32_t max_segments;
    uint32_t num_segments;

    struct block_cache_segment *segments;

    int fd;
};

int block_cache_init(struct block_cache *bc, int fd);
ssize_t block_cache_clear_valid(struct block_cache *bc, off_t offset, size_t count);
int block_cache_pwrite(struct block_cache *bc, const void *buf, size_t count, off_t offset, bool streamed);
int block_cache_pread(struct block_cache *bc, void *buf, size_t count, off_t offset);
int block_cache_flush(struct block_cache *bc);
int block_cache_free(struct block_cache *bc);

#endif // BLOCK_CACHE_H
