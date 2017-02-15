/*
 * Copyright 2014 LKC Technologies, Inc.
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

#ifndef ALIGNED_WRITER_H
#define ALIGNED_WRITER_H

#include <sys/types.h>

/*
 * The aligned_writer encapsulates the logic to write at raw disk
 * devices at aligned offsets and multiples of block_size whenever
 * possible. This significantly improves throughput on systems
 * that do not buffer writes.
 */
struct block_writer {
    int fd;
    size_t buffer_size;

    size_t block_size;
    off_t block_size_mask;

    char *unaligned_buffer;
    char *buffer;
    off_t write_offset;
    off_t last_write_offset;
    size_t buffer_index;
    size_t added_bytes;
};

int block_writer_init(struct block_writer *bw, int fd, int buffer_size, int log2_block_size);
ssize_t block_writer_pwrite(struct block_writer *aw, const void *buf, size_t count, off_t offset);
ssize_t block_writer_free(struct block_writer *aw);

#endif // ALIGNED_WRITER_H
