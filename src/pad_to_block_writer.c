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

#include "pad_to_block_writer.h"
#include "block_cache.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/**
 * The pad to block writer takes bytes written to any offset/size and aligns them
 * block boundaries. While it is possible to jump around when writing, this code
 * is not general purpose. All writes are expected to be sequential and possibly with
 * holes. There are boundary conditions that are not handled that should be handled
 * in a completely general purpose implementation. This can be seen in that the code
 * never issues any reads. This is ok, since the only use in fwup is to convert the
 * random byte-sized chunks from decompressors to block-sized chunks needed for writing
 * to disk.
 */

static size_t min(size_t a, size_t b)
{
    if (a < b)
        return a;
    else
        return b;
}

void ptbw_init(struct pad_to_block_writer *ptbw, struct block_cache *output)
{
    ptbw->output = output;
    ptbw->index = 0;
    ptbw->offset = 0;
}

int ptbw_pwrite(struct pad_to_block_writer *ptbw, const void *buf, size_t count, off_t offset)
{
    // Check for leftovers
    if (ptbw->index != 0) {
        // Add to the leftovers
        off_t current_index = ptbw->offset + (off_t) ptbw->index;
        off_t max_index = ptbw->offset + sizeof(ptbw->buffer);

        assert(offset >= current_index); // Writing to previous locations isn't supposed to be possible

        // Check if skipping bytes in a block.
        if (offset > current_index && offset < max_index) {
            // Fill skipped part with 0s
            size_t to_skip = offset - current_index;
            memset(&ptbw->buffer[ptbw->index], 0, to_skip);
            current_index = offset;
            ptbw->index += to_skip;
        }

        // Check if we're synced up.
        if (current_index == offset) {
            size_t to_copy = min(sizeof(ptbw->buffer) - ptbw->index, count);
            memcpy(&ptbw->buffer[ptbw->index], buf, to_copy);
            buf = (const uint8_t *) buf + to_copy;
            ptbw->index += to_copy;
            count -= to_copy;
            offset += to_copy;

            // Flush if filled or return if need more
            if (ptbw->index == sizeof(ptbw->buffer)) {
                // Write the full buffer out.
                OK_OR_RETURN(block_cache_pwrite(ptbw->output, ptbw->buffer, sizeof(ptbw->buffer), ptbw->offset, true));
                ptbw->index = 0;
            } else {
                // Still a partial buffer, so return until we get more.
                return 0;
            }
        } else {
            // Skipping forward, so flush what we have.
            OK_OR_RETURN(ptbw_flush(ptbw));
        }
    }

    // Verify that we're on a block boundary.
    size_t index_from_block_boundary = offset & (sizeof(ptbw->buffer) - 1);
    if (index_from_block_boundary != 0) {
        // If not, pad, and try again.
        memset(ptbw->buffer, 0, index_from_block_boundary);
        ptbw->index = index_from_block_boundary;
        ptbw->offset = offset - index_from_block_boundary;
        return ptbw_pwrite(ptbw, buf, count, offset);
    }

    if (count > sizeof(ptbw->buffer)) {
        // Write as many block size blocks as we can.
        size_t to_copy = (count & ~(sizeof(ptbw->buffer) - 1));
        OK_OR_RETURN(block_cache_pwrite(ptbw->output, buf, to_copy, offset, true));
        offset += to_copy;
        count -= to_copy;
        buf = (const uint8_t *) buf + to_copy;
    }

    // Any leftovers?
    if (count > 0) {
        memcpy(ptbw->buffer, buf, count);
        ptbw->index = count;
        ptbw->offset = offset;
    }

    return 0;
}

int ptbw_flush(struct pad_to_block_writer *ptbw)
{
    if (ptbw->index > 0) {
        // Clear out any unwritten parts of the block.
        memset(&ptbw->buffer[ptbw->index], 0, sizeof(ptbw->buffer) - ptbw->index);

        OK_OR_RETURN(block_cache_pwrite(ptbw->output, ptbw->buffer, sizeof(ptbw->buffer), ptbw->offset, true));

        ptbw->index = 0;
    }
    return 0;
}
