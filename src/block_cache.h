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

#ifndef BLOCK_CACHE_H
#define BLOCK_CACHE_H

#include "config.h"
#include "util.h"

// Don't use pthreads on Windows yet.
#ifndef _WIN32
#if HAVE_PTHREAD
#define USE_PTHREADS 1
#endif
#endif

#if USE_PTHREADS
#include <pthread.h>
#endif

// The segment size defines the minimum read/write size
// actually made to the output. Additionally, all reads and
// writes will be aligned to that size.
#define BLOCK_CACHE_SEGMENT_SIZE       (128*1024) // This is also the read/write write size
#define BLOCK_CACHE_NUM_SEGMENTS       64         // 8 MB cache
#define BLOCK_CACHE_BLOCKS_PER_SEGMENT (BLOCK_CACHE_SEGMENT_SIZE / FWUP_BLOCK_SIZE)
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

    // Bit fields for determining whether blocks inside the
    // segment are valid (hold the most up-to-date data) and/or
    // dirty (need to be written back to the target image).
    // (2 bits of flags in a uint8_t)
    uint8_t flags[BLOCK_CACHE_BLOCKS_PER_SEGMENT * 2 / 8];
};

struct block_cache {
    int fd;

    // Counter for maintaining LRU
    uint32_t timestamp;

    // All of the cached segments
    struct block_cache_segment segments[BLOCK_CACHE_NUM_SEGMENTS];

    // Temporary buffer for reading segments that are partially valid
    uint8_t *temp;

    // Track "trimmed" segments in a bitfield. One bit per segment.
    // E.g., 128K/bit -> 1M takes represented in 1 byte -> 1G in 1 KB, etc.
    size_t trimmed_len;
    off_t trimmed_end_offset;
    uint8_t *trimmed;
    bool trimmed_remainder; // true if segments after end of bitfield are trimmed
    bool hw_trim_enabled;

    // This tracks the number of blocks on the destination
    uint32_t num_blocks;

    // Asynchronous writes
#if USE_PTHREADS
    pthread_t writer_thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    volatile bool running;
    volatile struct block_cache_segment *seg_to_write;
    volatile off_t bad_offset; // set if pwrite fails asynchronously
#endif
};

int block_cache_init(struct block_cache *bc, int fd, off_t end_offset, bool enable_trim);
int block_cache_trim(struct block_cache *bc, off_t offset, off_t count, bool hwtrim);
int block_cache_trim_after(struct block_cache *bc, off_t offset, bool hwtrim);
int block_cache_pwrite(struct block_cache *bc, const void *buf, size_t count, off_t offset, bool streamed);
int block_cache_pread(struct block_cache *bc, void *buf, size_t count, off_t offset);
int block_cache_flush(struct block_cache *bc);
int block_cache_free(struct block_cache *bc);

#endif // BLOCK_CACHE_H
