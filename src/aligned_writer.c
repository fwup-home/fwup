#include "aligned_writer.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief initialize the aligned writer
 *
 * @param aw
 * @param fd
 * @param block_size
 * @return 0 if ok, -1 if not
 */
int aligned_writer_init(struct aligned_writer *aw, int fd, int log2_block_size)
{
    aw->fd = fd;
    aw->block_size = (1 << log2_block_size);
    aw->block_size_mask = ~((off_t) aw->block_size - 1);
    aw->buffer = (char *) malloc(aw->block_size);
    if (aw->buffer == 0)
        ERR_RETURN("Cannot allocate write buffer of %d bytes.", aw->block_size);

    aw->buffer_offset = 0;
    aw->buffer_count = 0;
    return 0;
}

static ssize_t flush_buffer(struct aligned_writer *aw)
{
    assert(aw->buffer_count <= aw->block_size); // Math failed somewhere if we're buffering more than the block_size

    ssize_t rc = pwrite(aw->fd, aw->buffer, aw->buffer_count, aw->buffer_offset);
    aw->buffer_offset += aw->buffer_count;
    aw->buffer_count = 0;
    return rc;
}

/**
 * @brief Write a number of bytes to an offset like pwrite
 *
 * The offset must be monotonically increasing, but gaps are supported
 * when dealing with sparse files.
 *
 * @param aw
 * @param buf
 * @param count
 * @param offset
 * @return
 */
ssize_t aligned_writer_pwrite(struct aligned_writer *aw, const void *buf, size_t count, off_t offset)
{
    ssize_t amount_written = 0;
    if (aw->buffer_count) {
        // If we're buffering, fill in the buffer to the alignment boundary.
        off_t position = aw->buffer_offset + aw->buffer_count;
        off_t boundary = (aw->buffer_offset + aw->block_size) & aw->block_size_mask;
        assert(position < boundary);
        size_t to_write = boundary - position;
        assert(offset >= position); // Check monotonicity of offset
        if (offset != position) {
            // Fill in the gap.
            off_t gap_size = offset - position;
            if (gap_size >= (off_t) to_write) {
                memset(&aw->buffer[aw->buffer_count], 0, to_write);
                aw->buffer_count += to_write;
                amount_written += aw->buffer_count;
                OK_OR_RETURN(flush_buffer(aw));
                goto empty_buffer;
            } else {
                // Fill in 0s for the gap, but we still need to buffer more.
                memset(&aw->buffer[aw->buffer_count], 0, gap_size);
                aw->buffer_count += gap_size;
                to_write -= gap_size;
            }
        }
        if (count < to_write) {
            // Not enough to write to disk, so buffer for next time
            memcpy(&aw->buffer[aw->buffer_count], buf, count);
            aw->buffer_count += count;
            return 0;
        } else {
            // Fill out the buffer
            memcpy(&aw->buffer[aw->buffer_count], buf, to_write);
            buf += to_write;
            count -= to_write;
            offset += to_write;
            aw->buffer_count += to_write;
            amount_written += aw->buffer_count;
            OK_OR_RETURN(flush_buffer(aw));
        }
    }

empty_buffer:
    assert(aw->buffer_count == 0);
    assert(offset >= aw->buffer_offset);

    // Advance to the offset
    aw->buffer_offset = offset;

    // Write in chunks of block_size and align.
    off_t boundary = (aw->buffer_offset + aw->block_size) & aw->block_size_mask;
    size_t to_write = boundary - aw->buffer_offset;
    while (count > to_write) {
        amount_written += to_write;
        if (pwrite(aw->fd, buf, to_write, aw->buffer_offset) < 0)
            return -1;
        aw->buffer_offset += to_write;
        buf += to_write;
        count -= to_write;
        boundary = (aw->buffer_offset + aw->block_size) & aw->block_size_mask;
        to_write = boundary - aw->buffer_offset;
    }

    // Buffer the left overs
    if (count) {
        memcpy(aw->buffer, buf, count);
        aw->buffer_count = count;
    }

    return amount_written;
}

/**
 * @brief flush and free the aligned writer data structures
 * @param aw
 * @return number of bytes written or -1 if error
 */
ssize_t aligned_writer_free(struct aligned_writer *aw)
{
    // Write any data that's still in the buffer
    ssize_t rc = 0;
    if (aw->buffer_count)
        rc = pwrite(aw->fd, aw->buffer, aw->buffer_count, aw->buffer_offset);

    // Free our buffer and clean up
    free(aw->buffer);
    aw->fd = -1;
    aw->buffer = 0;
    aw->buffer_count = 0;
    return rc;
}
