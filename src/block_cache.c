#include "block_cache.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static size_t min(size_t a, size_t b)
{
    if (a <= b)
        return a;
    else
        return b;
}

// Cache bit handling functions
static inline void set_dirty(struct block_cache_segment *seg, int block)
{
    // Dirty implies that the block is valid, so make sure that it's set too.
    uint8_t *bits = &seg->flags[block / 4];
    *bits = *bits | (0x3 << (2 * (block & 0x3)));
}
static void clear_all_dirty(struct block_cache_segment *seg)
{
    for (size_t i = 0; i < sizeof(seg->flags); i++)
        seg->flags[i] &= 0xaa;
}

static inline bool is_dirty(struct block_cache_segment *seg, int block)
{
    return (seg->flags[block / 4] & (0x1 << (2 * (block & 0x3)))) != 0;
}

static bool is_segment_dirty(struct block_cache_segment *seg)
{
    uint8_t orflags = 0;
    for (size_t i = 0; i < sizeof(seg->flags); i++)
        orflags = orflags | seg->flags[i];

    // Check if any dirty bit was set.
    return (orflags & 0x55) != 0;
}

static bool is_segment_completely_dirty(struct block_cache_segment *seg)
{
    uint8_t andflags = 0x55;
    for (size_t i = 0; i < sizeof(seg->flags); i++)
        andflags = andflags & seg->flags[i];

    // Check if any dirty bit wasn't set.
    return andflags == 0x55;
}
static void check_segment_validity(struct block_cache_segment *seg, bool *all_valid, bool *all_invalid)
{
    uint8_t andflags = 0xaa;
    uint8_t orflags = 0;
    for (size_t i = 0; i < sizeof(seg->flags); i++) {
        uint8_t flags = seg->flags[i];
        andflags = andflags & flags;
        orflags = andflags | flags;
    }

    *all_valid = (andflags == 0xaa);
    *all_invalid = ((orflags & 0xaa) == 0);
}

static inline void set_valid(struct block_cache_segment *seg, int block)
{
    uint8_t *bits = &seg->flags[block / 4];
    *bits = *bits | (0x2 << (2 * (block & 0x3)));
}

static inline bool is_valid(struct block_cache_segment *seg, int block)
{
    return (seg->flags[block / 4] & (0x2 << (2 * (block & 0x3)))) != 0;
}

static void init_segment(struct block_cache *bc, off_t offset, struct block_cache_segment *seg)
{
    void *data = seg->data;
    if (!data)
        alloc_page_aligned(&data, BLOCK_CACHE_SEGMENT_SIZE);

    memset(seg, 0, sizeof(*seg));

    seg->in_use = true;
    seg->data = data;
    seg->offset = offset;
    seg->last_access = bc->timestamp++;
    seg->streamed = true;
}

static int make_segment_valid(struct block_cache *bc, struct block_cache_segment *seg)
{
    bool all_valid;
    bool all_invalid;

    check_segment_validity(seg, &all_valid, &all_invalid);
    if (all_invalid) {
        // If completely invalid, read it all in. No merging necessary
        if (pread(bc->fd, seg->data, BLOCK_CACHE_SEGMENT_SIZE, seg->offset) != BLOCK_CACHE_SEGMENT_SIZE)
            ERR_RETURN("pread");
    } else if (!all_valid) {
        // Mixed valid/invalid. Need to read to a temporary buffer and merge.
        if (pread(bc->fd, bc->temp, BLOCK_CACHE_SEGMENT_SIZE, seg->offset) != BLOCK_CACHE_SEGMENT_SIZE)
            ERR_RETURN("pread");

        for (int i = 0; i < BLOCK_CACHE_BLOCKS_PER_SEGMENT; i++) {
            if (!is_valid(seg, i)) {
                size_t offset = i * BLOCK_SIZE;
                memcpy(seg->data + offset, bc->temp + offset, BLOCK_SIZE);
                set_valid(seg, i);
            }
        }
    }
    return 0;
}

static int flush_segment(struct block_cache *bc, struct block_cache_segment *seg)
{
    // Make sure that there's something to do.
    if (!seg->in_use || !is_segment_dirty(seg))
        return 0;

    OK_OR_RETURN(make_segment_valid(bc, seg));

    if (pwrite(bc->fd, seg->data, BLOCK_CACHE_SEGMENT_SIZE, seg->offset) != BLOCK_CACHE_SEGMENT_SIZE)
        ERR_RETURN("pwrite");

    clear_all_dirty(seg);

    return 0;
}

/**
 * @brief block_cache_init
 * @param bc
 * @param fd
 * @return
 */
int block_cache_init(struct block_cache *bc, int fd)
{
    memset(bc, 0, sizeof(struct block_cache));

    OK_OR_RETURN(block_writer_init(&bc->writer, fd, 120 * 1024, BLOCK_SIZE_LOG2));

    bc->fd = fd;
    alloc_page_aligned((void **) &bc->temp, BLOCK_CACHE_SEGMENT_SIZE);

    return 0;
}

int block_cache_flush(struct block_cache *bc)
{
    for (int i = 0; i < BLOCK_CACHE_NUM_SEGMENTS; i++)
        OK_OR_RETURN(flush_segment(bc, &bc->segments[i]));

    return 0;
}

/**
 * @brief Free all memory allocated by the block cache
 *
 * All cache segments are discarded, so be sure to call block_cache_flush
 * if it's important to keep them.
 *
 * @param bc
 * @return 0 on success
 */
int block_cache_free(struct block_cache *bc)
{
    for (int i = 0; i < BLOCK_CACHE_NUM_SEGMENTS; i++) {
        struct block_cache_segment *seg = &bc->segments[i];
        if (seg->data) {
            free_page_aligned(seg->data);
            seg->data = NULL;
            seg->in_use = false;
        }
    }
    free_page_aligned(bc->temp);
    bc->temp = NULL;
    return 0;
}

/**
 * Find the segment at the specified offset. If it doesn't exist, allocate
 * one, and if we've hit the max number of segments, discard the LRU.
 *
 * @param bc
 * @param offset
 * @param segment
 * @return
 */
static int get_segment(struct block_cache *bc, off_t offset, struct block_cache_segment **segment)
{
    // Check for a hit
    for (int i = 0; i < BLOCK_CACHE_NUM_SEGMENTS; i++) {
        struct block_cache_segment *seg = &bc->segments[i];
        if (seg->in_use && seg->offset == offset) {
            if (seg->write_in_progress)
                ; // TODO!!!block_writer_wait_for_completion();

            seg->last_access = bc->timestamp++;
            *segment = seg;
            return 0;
        }
    }

    // Cache miss, so either use an unused entry or the LRU
    struct block_cache_segment *lru = &bc->segments[0];
    if (!lru->in_use) {
        init_segment(bc, offset, lru);
        *segment = lru;
        return 0;
    }
    for (int i = 1; i < BLOCK_CACHE_NUM_SEGMENTS; i++) {
        struct block_cache_segment *seg = &bc->segments[i];
        if (!seg->in_use) {
            init_segment(bc, offset, seg);
            *segment = seg;
            return 0;
        }
        if (seg->last_access < lru->last_access)
            lru = seg;
    }

    OK_OR_RETURN(flush_segment(bc, lru));
    init_segment(bc, offset, lru);
    *segment = lru;
    return 0;
}

/**
 * @brief Clear out a range in the cache
 *
 * Additionally, mark these blocks so that they don't need to be written to
 * disk. If the format code writes * to them, they'll be marked dirty. However,
 * if any code tries to read them, they'll get back zeros without any I/O. This
 * is best effort.
 *
 * @param bc
 * @param offset
 * @param count
 * @return
 */
int block_cache_trim(struct block_cache *bc, off_t offset, size_t count)
{

    return 0;
}

static int block_segment_pwrite(struct block_cache *bc, struct block_cache_segment *seg, const void *buf, size_t count, size_t offset_into_segment, bool streamed)
{
    // Write the block to the cache
    memcpy(&seg->data[offset_into_segment], buf, count);

    // Mark everything that was written as dirty
    int block_start = offset_into_segment / BLOCK_SIZE;
    int block_end = block_start + count / BLOCK_SIZE;
    for (int i = block_start; i < block_end; i++)
        set_dirty(seg, i);

    // Check for the whole block streaming case where the best
    // strategy is to write it to flash immediately
    if (streamed && seg->streamed && is_segment_completely_dirty(seg)) {
        if (pwrite(bc->fd, seg->data, BLOCK_CACHE_SEGMENT_SIZE, seg->offset) != BLOCK_CACHE_SEGMENT_SIZE)
            ERR_RETURN("pwrite");

        // TODO: handle async

        // Mark everything valid.
        for (size_t i = 0; i < sizeof(seg->flags); i++)
            seg->flags[i] = 0xaa;
    } else {
        if (!streamed)
            seg->streamed = false;
    }

    return 0;
}

int block_cache_pwrite(struct block_cache *bc, const void *buf, size_t count, off_t offset, bool streamed)
{
    // Break into segment-sized chunks
    off_t first = offset & BLOCK_CACHE_SEGMENT_MASK;
    if (first != offset) {
        struct block_cache_segment *seg;
        OK_OR_RETURN(get_segment(bc, first, &seg));
        size_t offset_into_segment = offset - first;
        size_t segcount = min(count, BLOCK_CACHE_SEGMENT_SIZE - offset_into_segment);
        OK_OR_RETURN(block_segment_pwrite(bc, seg, buf, segcount, offset_into_segment, streamed));

        count -= segcount;
        offset += segcount;
        buf = (const char *) buf + segcount;
    }

    while (count > 0) {
        struct block_cache_segment *seg;
        OK_OR_RETURN(get_segment(bc, offset, &seg));

        size_t segcount = min(count, BLOCK_CACHE_SEGMENT_SIZE);
        OK_OR_RETURN(block_segment_pwrite(bc, seg, buf, segcount, 0, streamed));

        count -= segcount;
        offset += segcount;
        buf = (const char *) buf + segcount;
    }

    return 0;
}

static int block_segment_pread(struct block_cache *bc, struct block_cache_segment *seg, void *buf, size_t count, size_t offset_into_segment)
{
    // Update the cache
    OK_OR_RETURN(make_segment_valid(bc, seg));

    // Copy what we need
    memcpy(buf, &seg->data[offset_into_segment], count);

    return 0;
}

int block_cache_pread(struct block_cache *bc, void *buf, size_t count, off_t offset)
{
    // Break into segment-sized chunks
    off_t first = offset & BLOCK_CACHE_SEGMENT_MASK;
    if (first != offset) {
        struct block_cache_segment *seg;
        OK_OR_RETURN(get_segment(bc, first, &seg));
        size_t offset_into_segment = offset - first;
        size_t segcount = min(count, BLOCK_CACHE_SEGMENT_SIZE - offset_into_segment);
        OK_OR_RETURN(block_segment_pread(bc, seg, buf, segcount, offset_into_segment));

        count -= segcount;
        offset += segcount;
        buf = (char *) buf + segcount;
    }

    while (count > 0) {
        struct block_cache_segment *seg;
        OK_OR_RETURN(get_segment(bc, offset, &seg));

        size_t segcount = min(count, BLOCK_CACHE_SEGMENT_SIZE);
        OK_OR_RETURN(block_segment_pread(bc, seg, buf, segcount, 0));

        count -= segcount;
        offset += segcount;
        buf = (char *) buf + segcount;
    }

    return 0;
}
