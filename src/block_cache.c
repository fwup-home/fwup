/*
 * Copyright 2017 Frank Hunleth
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "block_cache.h"
#include "mmc.h"

#include <errno.h>
#include <inttypes.h>
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

// Trim handling functions
static void enlarge_trim_bitvector(struct block_cache *bc, size_t ix)
{
    // Round up the new length in 4K (aka 4GB chunks)
    size_t oldlength = bc->trimmed_len;
    size_t newlength = (ix + 4095) & ~4095;
    bc->trimmed = realloc(bc->trimmed, newlength);
    if (bc->trimmed == NULL)
        fwup_err(EXIT_FAILURE, "realloc");
    memset(bc->trimmed + oldlength,
           bc->trimmed_remainder ? 0xff : 00,
           newlength - oldlength);
    bc->trimmed_len = newlength;
}

static bool is_trimmed(struct block_cache *bc, off_t offset)
{
    size_t segment_ix = offset / BLOCK_CACHE_SEGMENT_SIZE;
    size_t bit_ix = segment_ix / 8;

    if (bit_ix >= bc->trimmed_len)
        return bc->trimmed_remainder;

    int bit = segment_ix & 7;
    return (bc->trimmed[bit_ix] & (1 << bit)) != 0;
}

static void clear_trimmed(struct block_cache *bc, off_t offset)
{
    size_t segment_ix = offset / BLOCK_CACHE_SEGMENT_SIZE;
    size_t bit_ix = segment_ix / 8;

    if (bit_ix >= bc->trimmed_len) {
        // If we're clearing the trim on something that's already not trimmed, then
        // we don't need to do anything.
        if (!bc->trimmed_remainder)
            return;

        // Expand the bit area to hold the new value
        enlarge_trim_bitvector(bc, bit_ix);
    }


    int bit = segment_ix & 7;
    bc->trimmed[bit_ix] &= ~(1 << bit);
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
        orflags = orflags | flags;
    }

    *all_valid = (andflags == 0xaa);
    *all_invalid = ((orflags & 0xaa) == 0);
}

static inline void set_valid(struct block_cache_segment *seg, int block)
{
    uint8_t *bits = &seg->flags[block / 4];
    *bits = *bits | (0x2 << (2 * (block & 0x3)));
}
static inline void set_all_valid(struct block_cache_segment *seg)
{
    memset(seg->flags, 0xaa, sizeof(seg->flags));
}

static inline bool is_valid(struct block_cache_segment *seg, int block)
{
    return (seg->flags[block / 4] & (0x2 << (2 * (block & 0x3)))) != 0;
}

// Hash table helper functions
static inline size_t hash_offset(off_t offset)
{
    // Segments are 128KB aligned (offset >> 17), hash to table size
    return ((size_t)(offset >> 17)) & (HASH_TABLE_SIZE - 1);
}

static void hash_insert(struct block_cache *bc, struct block_cache_segment *seg)
{
    size_t hash = hash_offset(seg->offset);
    seg->hash_next = bc->hash_table[hash];
    bc->hash_table[hash] = seg;
}

static void hash_remove(struct block_cache *bc, struct block_cache_segment *seg)
{
    size_t hash = hash_offset(seg->offset);
    struct block_cache_segment **ptr = &bc->hash_table[hash];
    
    while (*ptr) {
        if (*ptr == seg) {
            *ptr = seg->hash_next;
            seg->hash_next = NULL;
            return;
        }
        ptr = &(*ptr)->hash_next;
    }
}

static struct block_cache_segment *hash_lookup(struct block_cache *bc, off_t offset)
{
    size_t hash = hash_offset(offset);
    struct block_cache_segment *seg = bc->hash_table[hash];
    
    while (seg) {
        if (seg->offset == offset && seg->in_use)
            return seg;
        seg = seg->hash_next;
    }
    return NULL;
}

// LRU list helper functions
static void lru_remove(struct block_cache *bc, struct block_cache_segment *seg)
{
    if (seg->lru_prev)
        seg->lru_prev->lru_next = seg->lru_next;
    else
        bc->lru_head = seg->lru_next;
    
    if (seg->lru_next)
        seg->lru_next->lru_prev = seg->lru_prev;
    else
        bc->lru_tail = seg->lru_prev;
    
    seg->lru_prev = NULL;
    seg->lru_next = NULL;
}

static void lru_push_front(struct block_cache *bc, struct block_cache_segment *seg)
{
    seg->lru_next = bc->lru_head;
    seg->lru_prev = NULL;
    
    if (bc->lru_head)
        bc->lru_head->lru_prev = seg;
    else
        bc->lru_tail = seg;
    
    bc->lru_head = seg;
}

static void lru_touch(struct block_cache *bc, struct block_cache_segment *seg)
{
    // Move to front of LRU list (most recently used)
    if (bc->lru_head != seg) {
        lru_remove(bc, seg);
        lru_push_front(bc, seg);
    }
}

static void init_segment(struct block_cache *bc, off_t offset, struct block_cache_segment *seg)
{
    // Data buffer is already allocated, just reset the metadata
    memset(seg->flags, 0, sizeof(seg->flags));

    seg->in_use = true;
    seg->offset = offset;
    seg->last_access = bc->timestamp++;
    seg->streamed = true;
    seg->hash_next = NULL;
    
    // Add to hash table
    hash_insert(bc, seg);
    
    // Move to front of LRU list
    lru_push_front(bc, seg);
}

static int calculate_io_size(struct block_cache *bc, off_t offset, size_t *count)
{
    off_t last_offset = offset + BLOCK_CACHE_SEGMENT_SIZE;

    // If there's a max destination size, then check if it's been hit or
    // whether this should be a partial write.
    if (bc->end_offset > 0 && !bc->is_soft_end_offset) {
        if (last_offset <= bc->end_offset) {
            // Common case: reading or writing before the end
            *count = BLOCK_CACHE_SEGMENT_SIZE;
        } else if (last_offset > bc->end_offset) {
            // At least some reads or writes are after the end
            if (offset <= bc->end_offset) {
                // Shrink the read/write count to support the partial segment
                *count = bc->end_offset - offset;
            } else {
                // Fail if the write is completely past the end
                *count = 0;
                ERR_RETURN("read/write failed at offset %" PRId64 " since past end of media (%" PRId64 ").", offset, bc->end_offset);
            }
        }
    } else {
        // Regular file with unknown max size. Just read or write 128KB and see
        // what happens. Reads will be truncated by the OS and writes will
        // extend the file.
        *count = BLOCK_CACHE_SEGMENT_SIZE;
        if (bc->is_soft_end_offset && bc->end_offset > 0 && last_offset > bc->end_offset) {
            // If we're writing past the end of a soft end offset, then push the soft end offset
            // to the new end of file.
            bc->end_offset = last_offset;
        }
    }

    return 0;
}

static int read_segment(struct block_cache *bc, struct block_cache_segment *seg, void *data)
{
    if (is_trimmed(bc, seg->offset)) {
        // Trimmed, so we'd be reading uninitialized data (in theory), if we called pread.
        memset(data, 0, BLOCK_CACHE_SEGMENT_SIZE);
    } else {
        size_t count;
        OK_OR_RETURN(calculate_io_size(bc, seg->offset, &count));
        ssize_t bytes_read = pread(bc->fd, data, count, seg->offset);
        if (bytes_read < 0) {
            ERR_RETURN("unexpected error reading %zu bytes at offset %" PRId64 ": %s.\nPossible causes are that the destination is too small, the device (e.g., an SD card) is going bad, or the connection to it is flaky.",
                    count, seg->offset, strerror(errno));
        } else if (bytes_read < BLOCK_CACHE_SEGMENT_SIZE) {
            // Didn't read enough bytes. This occurs if the destination media is
            // not a multiple of the segment size. Fill the remainder with zeros
            // and don't fail.
            memset((uint8_t *) data + bytes_read, 0, BLOCK_CACHE_SEGMENT_SIZE - bytes_read);
        }
    }
    return 0;
}

static int make_segment_valid(struct block_cache *bc, struct block_cache_segment *seg)
{
    bool all_valid;
    bool all_invalid;

    check_segment_validity(seg, &all_valid, &all_invalid);
    if (all_invalid) {
        // If completely invalid, read it all in. No merging necessary
        OK_OR_RETURN(read_segment(bc, seg, seg->data));
        set_all_valid(seg);
    } else if (!all_valid) {
        // Mixed valid/invalid. Need to read to a temporary buffer and merge.
        OK_OR_RETURN(read_segment(bc, seg, bc->read_temp));

        for (int i = 0; i < BLOCK_CACHE_BLOCKS_PER_SEGMENT; i++) {
            if (!is_valid(seg, i)) {
                size_t offset = i * FWUP_BLOCK_SIZE;
                memcpy(seg->data + offset, bc->read_temp + offset, FWUP_BLOCK_SIZE);
                set_valid(seg, i);
            }
        }
    }
    return 0;
}

static int verified_segment_write(struct block_cache *bc, volatile struct block_cache_segment *seg, uint8_t *temp)
{
    off_t offset = seg->offset;
    const uint8_t *data = seg->data;

    size_t count;
    OK_OR_RETURN(calculate_io_size(bc, seg->offset, &count));

    if (bc->minimize_writes) {
        if (pread(bc->fd, temp, count, offset) == count &&
            memcmp(data, temp, count) == 0) {
            return 0;
        }
    }

    if (pwrite(bc->fd, data, count, offset) != count)
        ERR_RETURN("writing %zu bytes failed at offset %" PRId64 ". Check media size.", count, offset);

    if (bc->verify_writes) {
        if (pread(bc->fd, temp, count, offset) != count)
            ERR_RETURN("read back failed at offset %" PRId64, offset);

        if (memcmp(data, temp, count) != 0)
            ERR_RETURN("write verification failed at offset %" PRId64, offset);
    }

    return 0;
}

#if USE_PTHREADS
static void *writer_worker(void *void_bc)
{
    struct block_cache *bc = (struct block_cache *) void_bc;

    OK_OR_FAIL(pthread_mutex_lock(&bc->mutex));
    for (;;) {
        // Check if there's work in the queue
        if (bc->write_queue_head != bc->write_queue_tail) {
            volatile struct block_cache_segment *seg = bc->write_queue[bc->write_queue_tail];
            bc->write_queue[bc->write_queue_tail] = NULL;
            bc->write_queue_tail = (bc->write_queue_tail + 1) % WRITE_QUEUE_SIZE;
            OK_OR_FAIL(pthread_cond_broadcast(&bc->cond));

            // Skip the write if there was a previous write error
            // A negative value for bc->bad_offset indicates no error has occurred.
            if (bc->bad_offset < 0) {
                OK_OR_FAIL(pthread_mutex_unlock(&bc->mutex));
                if (verified_segment_write(bc, seg, bc->thread_verify_temp) < 0)
                    bc->bad_offset = seg->offset;
                OK_OR_FAIL(pthread_mutex_lock(&bc->mutex));
            }

            OK_OR_FAIL(pthread_cond_broadcast(&bc->cond));
        } else {
            if (!bc->running)
                break;

            OK_OR_FAIL(pthread_cond_wait(&bc->cond, &bc->mutex));
        }
    }
    pthread_mutex_unlock(&bc->mutex);
    return NULL;
}
static int check_async_error(struct block_cache *bc)
{
    if (bc->bad_offset >= 0)
        ERR_RETURN("write failed at offset %" PRId64". Check media size.", bc->bad_offset);
    return 0;
}

static bool is_segment_in_write_queue(struct block_cache *bc, struct block_cache_segment *seg)
{
    size_t pos = bc->write_queue_tail;
    while (pos != bc->write_queue_head) {
        if (bc->write_queue[pos] == seg)
            return true;
        pos = (pos + 1) % WRITE_QUEUE_SIZE;
    }
    return false;
}

static int do_async_write(struct block_cache *bc, struct block_cache_segment *seg)
{
    // Don't start if already errored.
    OK_OR_RETURN(check_async_error(bc));

    OK_OR_FAIL(pthread_mutex_lock(&bc->mutex));
    
    // Wait if the queue is full
    size_t next_head = (bc->write_queue_head + 1) % WRITE_QUEUE_SIZE;
    while (next_head == bc->write_queue_tail)
        OK_OR_FAIL(pthread_cond_wait(&bc->cond, &bc->mutex));
    
    // Add to queue
    bc->write_queue[bc->write_queue_head] = seg;
    bc->write_queue_head = next_head;
    
    OK_OR_FAIL(pthread_cond_broadcast(&bc->cond));
    OK_OR_FAIL(pthread_mutex_unlock(&bc->mutex));

    // NOTE: this check is best effort. If it catches something it will almost certainly
    //       be a previous write.
    return check_async_error(bc);
}
static void wait_for_write_completion(struct block_cache *bc, struct block_cache_segment *seg)
{
    // Wait for write thread to finish processing this segment
    OK_OR_FAIL(pthread_mutex_lock(&bc->mutex));
    while (is_segment_in_write_queue(bc, seg))
        OK_OR_FAIL(pthread_cond_wait(&bc->cond, &bc->mutex));
    OK_OR_FAIL(pthread_mutex_unlock(&bc->mutex));
}

static inline int do_sync_write(struct block_cache *bc, struct block_cache_segment *seg)
{
    // Don't start if already errored.
    OK_OR_RETURN(check_async_error(bc));

    OK_OR_FAIL(pthread_mutex_lock(&bc->mutex));
    if (is_segment_in_write_queue(bc, seg)) {
        // Wait for async write to complete
        while (is_segment_in_write_queue(bc, seg))
            OK_OR_FAIL(pthread_cond_wait(&bc->cond, &bc->mutex));

        OK_OR_FAIL(pthread_mutex_unlock(&bc->mutex));
        return check_async_error(bc);
    } else {
        OK_OR_FAIL(pthread_mutex_unlock(&bc->mutex));
        int rc = verified_segment_write(bc, seg, bc->verify_temp);
        if (rc < 0)
            bc->bad_offset = seg->offset;
        return rc;
    }
}
#else
// Single-threaded version
static inline int do_sync_write(struct block_cache *bc, struct block_cache_segment *seg)
{
    return verified_segment_write(bc, seg, bc->verify_temp);
}
static inline int do_async_write(struct block_cache *bc, struct block_cache_segment *seg)
{
    return do_sync_write(bc, seg);
}
static inline void wait_for_write_completion(struct block_cache *bc, struct block_cache_segment *seg)
{
    // Not async, so no waits.
    (void) bc;
    (void) seg;
}
#endif

static int flush_segment(struct block_cache *bc, struct block_cache_segment *seg)
{
    // Make sure that there's something to do.
    if (!seg->in_use || !is_segment_dirty(seg))
        return 0;

    // Try to write the segment out. If it is partial, do a read/modify/write
    int rc = 0;
    OK_OR_CLEANUP(make_segment_valid(bc, seg));
    OK_OR_CLEANUP(do_sync_write(bc, seg));

cleanup:
    // On success, the block isn't dirty and it's no longer trimmed.
    // On error, the logic is to do the same thing since we don't want this
    // block repeatedly stuck dirty and hopelessly retried. Hopefully the error
    // gets handled by the caller of fwup to take appropriate action, though.
    clear_all_dirty(seg);
    clear_trimmed(bc, seg->offset);

    return rc;
}

/**
 * @brief block_cache_init
 * @param bc
 * @param fd the file descriptor of the destination
 * @param end_offset the size of the destination in bytes or 0 if unknown
 * @param is_soft_end_offset true if the end_offset is a soft limit and writes can go past it
 * @param enable_trim true if allowed to issue TRIM commands to the device
 * @param verify_writes true to verify writes
 * @param minimize_writes true to read the block before writing and skip the write if it's the same
 * @param cache_size_mb the size of the block cache in MB (0 for auto-detect)
 * @return
 */
int block_cache_init(struct block_cache *bc,
        int fd,
        off_t end_offset,
        bool is_soft_end_offset,
        bool enable_trim,
        bool verify_writes,
        bool minimize_writes,
        size_t cache_size_mb)
{
    memset(bc, 0, sizeof(struct block_cache));

    // Determine cache size
    if (cache_size_mb == 0) {
        cache_size_mb = get_configured_cache_size_mb();
    }

    // Calculate number of segments
    bc->num_segments = (cache_size_mb * 1024 * 1024) / BLOCK_CACHE_SEGMENT_SIZE;
    if (bc->num_segments < 8) {
        bc->num_segments = 8; // Minimum 1 MB cache
    }

    INFO("Initializing block cache: %zu MB (%zu segments)", cache_size_mb, bc->num_segments);

    // Allocate segments array
    bc->segments = (struct block_cache_segment *) calloc(bc->num_segments, sizeof(struct block_cache_segment));
    if (bc->segments == NULL)
        fwup_err(EXIT_FAILURE, "calloc segments array");

    // Initialize hash table to NULL
    for (size_t i = 0; i < HASH_TABLE_SIZE; i++)
        bc->hash_table[i] = NULL;

    // Initialize LRU list
    bc->lru_head = NULL;
    bc->lru_tail = NULL;

    // Pre-allocate ALL segment data buffers upfront to avoid runtime allocation
    INFO("Pre-allocating %zu MB of segment buffers", (bc->num_segments * BLOCK_CACHE_SEGMENT_SIZE) / (1024 * 1024));
    for (size_t i = 0; i < bc->num_segments; i++) {
        alloc_page_aligned((void **) &bc->segments[i].data, BLOCK_CACHE_SEGMENT_SIZE);
        if (bc->segments[i].data == NULL)
            fwup_err(EXIT_FAILURE, "alloc_page_aligned segment data");
        
        // Initialize segment metadata
        bc->segments[i].in_use = false;
        bc->segments[i].hash_next = NULL;
        bc->segments[i].lru_prev = NULL;
        bc->segments[i].lru_next = NULL;
    }

#if USE_PTHREADS
    bc->running = true;
    bc->bad_offset = -1;
    bc->write_queue_head = 0;
    bc->write_queue_tail = 0;
    for (size_t i = 0; i < WRITE_QUEUE_SIZE; i++)
        bc->write_queue[i] = NULL;

    pthread_mutex_init(&bc->mutex, NULL);
    pthread_cond_init(&bc->cond, NULL);
    if (verify_writes)
        alloc_page_aligned((void **) &bc->thread_verify_temp, BLOCK_CACHE_SEGMENT_SIZE);
#endif

    bc->fd = fd;
    bc->verify_writes = verify_writes;
    bc->minimize_writes = minimize_writes;
    alloc_page_aligned((void **) &bc->read_temp, BLOCK_CACHE_SEGMENT_SIZE);

    if (verify_writes || minimize_writes)
        alloc_page_aligned((void **) &bc->verify_temp, BLOCK_CACHE_SEGMENT_SIZE);

    // Initialized to nothing trimmed. I.e. every write that doesn't fall on a
    // segment boundary is a read/modify/write.
    bc->trimmed_remainder = false;
    bc->trimmed_len = 64 * 1024; // 64K * 8 bits/byte * 128K bytes/segment = 64G start trimmed area tracking size
    bc->trimmed_end_offset = ((off_t) bc->trimmed_len) * 8 * BLOCK_CACHE_SEGMENT_SIZE;
    bc->trimmed = (uint8_t *) malloc(bc->trimmed_len);
    if (bc->trimmed == NULL)
        fwup_err(EXIT_FAILURE, "malloc");
    memset(bc->trimmed, 0, bc->trimmed_len);
    bc->hw_trim_enabled = enable_trim;
    bc->end_offset = end_offset;
    bc->is_soft_end_offset = is_soft_end_offset;

    // Set the trim points based on the file size
    if (!is_soft_end_offset && end_offset > 0) {
        // Mark the trim data structure that everything past the end has been trimmed.
        off_t aligned_end_offset = (end_offset + BLOCK_CACHE_SEGMENT_SIZE - 1) & BLOCK_CACHE_SEGMENT_MASK;
        OK_OR_RETURN(block_cache_trim_after(bc, aligned_end_offset, false));
    } else {
        // When the device size is unknown, don't try to initialize the trim
        // bit vector to optimize reads past the end. This really only helps
        // for regular files with partial segment writes at the end, so not a
        // big deal.
    }

    // Start async writer thread if available
#if USE_PTHREADS
    if (pthread_create(&bc->writer_thread, NULL, writer_worker, bc))
        fwup_errx(EXIT_FAILURE, "pthread_create");
#endif

    return 0;
}

static int lrucompare(const void *pa, const void *pb)
{
    const struct block_cache_segment *a = *((const struct block_cache_segment **) pa);
    const struct block_cache_segment *b = *((const struct block_cache_segment **) pb);

    if (!a->in_use && !b->in_use)
        return 0;

    if (!a->in_use)
        return 1;
    if (!b->in_use)
        return -1;

    return a->last_access < b->last_access ? -1 : 1;
}

void block_cache_reset(struct block_cache *bc)
{
    // Throw away everything in the cache. This is only called on errors so
    // that any writes that we still control can be cancelled to minimize
    // changes. The block cache can be used again for error handling code.

    for (size_t i = 0; i < bc->num_segments; i++) {
        struct block_cache_segment *seg = &bc->segments[i];
        if (seg->in_use) {
            wait_for_write_completion(bc, seg);
            
            // Remove from hash table and LRU list
            hash_remove(bc, seg);
            lru_remove(bc, seg);
            
            seg->in_use = false;
        }
    }
#if USE_PTHREADS
    bc->bad_offset = -1;
#endif
}

int block_cache_flush(struct block_cache *bc)
{
    // Blocks must be written back from the one that was written first to the one that
    // was written most recently. This has one important purpose:
    //
    // The ordering specified in the fwup.conf file is mostly preserved.
    // E.g., if there's an A/B partition switch done last in the fwup.conf,
    // it will be done last as part of the flush. This provides some
    // confidence that if the system crashes before the final write, the
    // A/B switch won't occur.
    //
    // Note that write order is "mostly" preserved since the cache converts the
    // fwup configuration's view of writes (mkfs, MBR operations, raw writes, etc.)
    // into 128 KB block operations. One 128 KB block can be the target of more
    // than FAT operation or raw write, and when that happens, the most recent one
    // drives the final sort order.

    struct block_cache_segment **sorted_segments = (struct block_cache_segment **) malloc(bc->num_segments * sizeof(struct block_cache_segment *));
    if (sorted_segments == NULL)
        fwup_err(EXIT_FAILURE, "malloc sorted_segments");
    
    for (size_t i = 0; i < bc->num_segments; i++)
        sorted_segments[i] = &bc->segments[i];
    qsort(sorted_segments, bc->num_segments, sizeof(struct block_cache_segment *), lrucompare);

    int rc = 0;
    for (size_t i = 0; i < bc->num_segments; i++) {
        if (flush_segment(bc, sorted_segments[i]) < 0) {
            rc = -1;
            break;
        }
    }

    free(sorted_segments);
    return rc;
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
#if USE_PTHREADS
    // Wait for the most recent async write to complete and
    // signal that the thread should exit.
    pthread_mutex_lock(&bc->mutex);
    bc->running = false;
    pthread_cond_broadcast(&bc->cond);
    pthread_mutex_unlock(&bc->mutex);

    if (pthread_join(bc->writer_thread, NULL))
        fwup_errx(EXIT_FAILURE, "pthread_join");
    pthread_mutex_destroy(&bc->mutex);
    pthread_cond_destroy(&bc->cond);
    if (bc->thread_verify_temp)
        free_page_aligned(bc->thread_verify_temp);
    bc->thread_verify_temp = NULL;
#endif

    for (size_t i = 0; i < bc->num_segments; i++) {
        struct block_cache_segment *seg = &bc->segments[i];
        if (seg->data) {
            free_page_aligned(seg->data);
            seg->data = NULL;
            seg->in_use = false;
        }
    }
    free_page_aligned(bc->read_temp);
    if (bc->verify_temp)
        free_page_aligned(bc->verify_temp);
    free(bc->trimmed);
    free(bc->segments);

    bc->segments = NULL;
    bc->trimmed = NULL;
    bc->read_temp = NULL;
    bc->verify_temp = NULL;
    bc->fd = -1;
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
    // O(1) hash table lookup for cache hit
    struct block_cache_segment *seg = hash_lookup(bc, offset);
    if (seg) {
        // Cache hit! Wait for async writes to complete on this segment before use.
        wait_for_write_completion(bc, seg);

        seg->last_access = bc->timestamp++;
        lru_touch(bc, seg);  // Move to front of LRU list
        *segment = seg;
        return 0;
    }

    // Cache miss - find an unused segment
    for (size_t i = 0; i < bc->num_segments; i++) {
        if (!bc->segments[i].in_use) {
            init_segment(bc, offset, &bc->segments[i]);
            *segment = &bc->segments[i];
            return 0;
        }
    }

    // No unused segments - evict the LRU (tail of list)
    struct block_cache_segment *lru = bc->lru_tail;
    if (!lru) {
        // Should never happen if num_segments > 0
        ERR_RETURN("LRU list is empty but cache is full");
    }

    // Remove from hash table and LRU list before reusing
    hash_remove(bc, lru);
    lru_remove(bc, lru);

    // Flush the segment if dirty
    OK_OR_RETURN(flush_segment(bc, lru));

    // Reinitialize for new offset (also adds back to hash and LRU)
    init_segment(bc, offset, lru);
    *segment = lru;
    return 0;
}

/**
 * @brief Clear out a range in the cache
 *
 * Additionally, mark these blocks so that they don't need to be written to
 * disk. If the format code writes to them, they'll be marked dirty. However,
 * if any code tries to read them, they'll get back zeros without any I/O. This
 * is best effort.
 *
 * @param bc
 * @param offset the byte offset for where to start
 * @param count how many bytes to trim
 * @param hwtrim true to issue a trim command to the memory device
 * @return
 */
int block_cache_trim(struct block_cache *bc, off_t offset, off_t count, bool hwtrim)
{
    // Force the offset and count to segment boundaries. Since
    // trimming is best effort, ignore sub boundary areas.
    // E.g., round the offset up and the count down.
    off_t aligned_offset = (offset + BLOCK_CACHE_SEGMENT_SIZE - 1) & BLOCK_CACHE_SEGMENT_MASK;
    count -= aligned_offset - offset;
    count = count & BLOCK_CACHE_SEGMENT_MASK;
    if (count <= 0)
        return 0;

    // Only trim the bit vector if within range
    if (offset < bc->trimmed_end_offset) {
        // Adjust the count to be within the bit vector
        size_t adjusted_count;
        if (offset + count <= bc->trimmed_end_offset)
            adjusted_count = count;
        else
            adjusted_count = (size_t) (bc->trimmed_end_offset - offset);

        size_t begin_segment_ix = aligned_offset / BLOCK_CACHE_SEGMENT_SIZE;
        size_t begin_bit_ix = begin_segment_ix / 8;
        size_t end_segment_ix = begin_segment_ix + adjusted_count / BLOCK_CACHE_SEGMENT_SIZE;
        size_t end_bit_ix = end_segment_ix / 8;

        // Check if the trim region needs expanding
        if (end_bit_ix > bc->trimmed_len) {
            if (bc->trimmed_remainder) {
                // If the end is already trimmed, then adjust the marking region
                end_segment_ix = bc->trimmed_len * 8;
                end_bit_ix = bc->trimmed_len;

                // Check whether end got trimmed before the beginning.
                if (begin_bit_ix > end_bit_ix)
                    return 0;
            } else {
                // If not, then expand the bit array.
                enlarge_trim_bitvector(bc, end_bit_ix);
            }
        }

        // Set bits
        if (begin_bit_ix == end_bit_ix) {
            // Only set a few bits
            for (size_t bit = begin_segment_ix & 7;
                 bit < (end_segment_ix & 7);
                 bit++) {
                bc->trimmed[begin_bit_ix] |= (1 << bit);
            }
        } else {
            // Set bits in bulk.

            // Handle the first byte
            for (size_t bit = begin_segment_ix & 7;
                 bit < 8;
                 bit++) {
                bc->trimmed[begin_bit_ix] |= (1 << bit);
            }
            begin_bit_ix++;

            // Handle any middle bytes
            if (end_bit_ix > begin_bit_ix)
                memset(&bc->trimmed[begin_bit_ix], 0xff, end_bit_ix - begin_bit_ix);

            // Handle the last byte
            for (size_t bit = 0;
                 bit < (end_segment_ix & 7);
                 bit++)
                bc->trimmed[end_bit_ix] |= (1 << bit);
        }

        // Trim out anything in the cache
        for (size_t i = 0; i < bc->num_segments; i++) {
            struct block_cache_segment *seg = &bc->segments[i];
            if (seg->in_use && seg->offset >= aligned_offset && seg->offset < aligned_offset + (off_t) count) {
                // Wait for writes to complete on this segment before letting it be used again.
                wait_for_write_completion(bc, seg);

                // Remove from hash table and LRU list
                hash_remove(bc, seg);
                lru_remove(bc, seg);

                // Return the segment
                seg->in_use = false;
            }
        }
    }

    // Try to issue a trim to the storage device. This is best effort, so if
    // not supported, it's no big deal.
    if (bc->hw_trim_enabled && hwtrim)
        mmc_trim(bc->fd, aligned_offset, count);

    return 0;
}

/**
 * @brief Trim everything including and after the specified offset.
 *
 * @param bc
 * @param offset
 * @return
 */
int block_cache_trim_after(struct block_cache *bc, off_t offset, bool hwtrim)
{
    if (offset > bc->trimmed_end_offset) {
        // The offset is in a range that we don't track with the bit vector,
        // so don't do anything to the cache datastructures.

        // IMPROVEMENT: Issue a trim to the disk for the offset and afterwards.

        return 0;
    }

    // Trim everything after what we keep track of
    bc->trimmed_remainder = true;

    // Trim out all blocks in the cache.
    for (size_t i = 0; i < bc->num_segments; i++) {
        struct block_cache_segment *seg = &bc->segments[i];
        if (seg->in_use && seg->offset >= offset) {
            // Wait for writes to complete on this segment before letting it be used again.
            wait_for_write_completion(bc, seg);

            // Remove from hash table and LRU list
            hash_remove(bc, seg);
            lru_remove(bc, seg);

            // Return the segment
            seg->in_use = false;
        }
    }

    return block_cache_trim(bc, offset, bc->trimmed_end_offset - (size_t) offset, hwtrim);
}

static int block_segment_pwrite(struct block_cache *bc, struct block_cache_segment *seg, const void *buf, size_t count, size_t offset_into_segment, bool streamed)
{
    // Write the block to the cache
    memcpy(&seg->data[offset_into_segment], buf, count);

    // Mark everything that was written as dirty
    int block_start = offset_into_segment / FWUP_BLOCK_SIZE;
    int block_end = block_start + count / FWUP_BLOCK_SIZE;
    for (int i = block_start; i < block_end; i++)
        set_dirty(seg, i);

    // Check for the whole block streaming case where the best
    // strategy is to write it to flash immediately
    if (streamed && seg->streamed && is_segment_completely_dirty(seg)) {
        OK_OR_RETURN(do_async_write(bc, seg));

        // Mark everything valid.
        for (size_t i = 0; i < sizeof(seg->flags); i++)
            seg->flags[i] = 0xaa;

        clear_trimmed(bc, seg->offset);
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
    size_t count_left = count;
    off_t first = offset & BLOCK_CACHE_SEGMENT_MASK;
    if (first != offset) {
        struct block_cache_segment *seg;
        OK_OR_RETURN(get_segment(bc, first, &seg));
        size_t offset_into_segment = offset - first;
        size_t segcount = min(count_left, BLOCK_CACHE_SEGMENT_SIZE - offset_into_segment);
        OK_OR_RETURN(block_segment_pread(bc, seg, buf, segcount, offset_into_segment));

        count_left -= segcount;
        offset += segcount;
        buf = (char *) buf + segcount;
    }

    while (count_left > 0) {
        struct block_cache_segment *seg;
        OK_OR_RETURN(get_segment(bc, offset, &seg));

        size_t segcount = min(count_left, BLOCK_CACHE_SEGMENT_SIZE);
        OK_OR_RETURN(block_segment_pread(bc, seg, buf, segcount, 0));

        count_left -= segcount;
        offset += segcount;
        buf = (char *) buf + segcount;
    }

    return count;
}
