#include "pad_to_block_writer.h"
#include "block_cache.h"

#include <stdlib.h>
#include <string.h>
#include <err.h>

/**
 * The pad to block writer takes bytes written to any offset/size and aligns them
 * block boundaries. While it is possible to jump around when writing, this code
 * is not general purpose. All writes are expected to be sequentual and possibly with
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
        if (ptbw->offset + (off_t) ptbw->index == offset) {
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

    // Verify that we're on a block boundary. That's an assumption that if broken will
    // result in corruption, but it's easy to make since it's not possible to specify
    // non-block boundaries in fwup.conf files and holes are multiple of block sizes.
    if (offset & (sizeof(ptbw->buffer) - 1))
        errx(EXIT_FAILURE, "unexpected non-block offset detected: %lu", offset);

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
