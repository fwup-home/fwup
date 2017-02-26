#include "block_writer.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if USE_PTHREADS
static void *writer_worker(void *void_bw)
{
    struct block_writer *bw = (struct block_writer *) void_bw;

    for (;;) {
        pthread_mutex_lock(&bw->mutex_to);

        while (bw->amount_to_write > 0) {
            ssize_t rc = write(bw->fd, bw->async_buffer, bw->amount_to_write);
            if (rc < 0) {
                fwup_errx(EXIT_FAILURE, "Need to handle this");

            }
            bw->amount_to_write -= rc;
        }

        if (!bw->running)
            break;

        pthread_mutex_unlock(&bw->mutex_back);
    }
    return NULL;
}
static ssize_t do_write(struct block_writer *bw, const char *buf, size_t count)
{
    // Wait for the writer thread to complete the last set of writes
    // before starting new ones.
    pthread_mutex_lock(&bw->mutex_back);

    // Only write up to bw->buffer_size asynchronously. Any more is
    // done here.
    size_t count_to_write_asynchronously = bw->buffer_size;

    if (bw->write_offset != bw->last_write_offset) {
        if (lseek(bw->fd, bw->write_offset, SEEK_SET) < 0) {
            pthread_mutex_unlock(&bw->mutex_back);
            return -1;
        }

        bw->last_write_offset = bw->write_offset;
    }

    size_t amount_left = count;
    while (amount_left > count_to_write_asynchronously) {
        ssize_t rc = write(bw->fd, buf, bw->buffer_size);
        if (rc <= 0) {
            pthread_mutex_unlock(&bw->mutex_back);
            return -1;
        }

        bw->last_write_offset += rc;
        buf += rc;
        amount_left -= rc;
    }
    memcpy(bw->async_buffer, buf, amount_left);
    bw->amount_to_write = amount_left;
    pthread_mutex_unlock(&bw->mutex_to);

    return count;
}
#else
// Single-threaded version
static ssize_t do_write(struct block_writer *bw, const char *buf, size_t count)
{
    if (bw->write_offset != bw->last_write_offset) {
        if (lseek(bw->fd, bw->write_offset, SEEK_SET) < 0)
            return -1;

        bw->last_write_offset = bw->write_offset;
    }

    size_t amount_left = count;
    while (amount_left) {
        ssize_t rc = write(bw->fd, buf, amount_left);
        if (rc <= 0)
            return -1;

        bw->last_write_offset += rc;
        buf += rc;
        amount_left -= rc;
    }

    return count;
}
#endif

/**
 * @brief Helper for buffering writes into block-sized pieces
 *
 * This is needed since the unzip process creates variable sized chunks that need
 * to be combined before being written. File I/O to Flash media works best in large
 * sized chunks, so cache the writes until they get to that size. Writes MUST be
 * monotically increasing in offset. Seeking backwards is NOT supported.
 *
 * Based on benchmarks and inspection of how Linux writes to Flash media at a low level,
 * the chunks don't need to be on Flash erase block boundaries or Flash erase block size.
 * This doesn't appear to hurt, but I found out that Linux was sending 120 KB blocks (NOT
 * 128 KB blocks) via a USB sniffer no matter how much larger blocks were written. Performance
 * was still very fast.
 *
 * This code also pads files out to block boundaries (512 bytes for all supported media now).
 * This is needed on OSX when writing to /dev/rdiskN devices.
 *
 * @param aw
 * @param fd              the target file to write to
 * @param buffer_size     the size of the buffer (rounded to multiple of 2^log2_block_size)
 * @param log2_block_size 9 for 512 byte blocks
 * @return 0 if ok, -1 if not
 */
int block_writer_init(struct block_writer *bw, int fd, int buffer_size, int log2_block_size)
{
    bw->fd = fd;
    bw->block_size = (1 << log2_block_size);
    bw->block_size_mask = ~((off_t) bw->block_size - 1);
    bw->buffer_size = (buffer_size + ~bw->block_size_mask) & bw->block_size_mask;

    // Buffer alignment required on Linux when files are opened with O_DIRECT.
    if (alloc_page_aligned((void **) &bw->buffer, bw->buffer_size) < 0)
        return 1;

    bw->write_offset = 0;
    bw->last_write_offset = -1;
    bw->buffer_index = 0;
    bw->added_bytes = 0;

#if USE_PTHREADS
    if (alloc_page_aligned((void **) &bw->async_buffer, bw->buffer_size) < 0) {
        free(bw->buffer);
        return -1;
    }
    bw->running = true;

    pthread_mutex_init(&bw->mutex_to, NULL);
    pthread_mutex_lock(&bw->mutex_to);
    pthread_mutex_init(&bw->mutex_back, NULL);
    if (pthread_create(&bw->writer_thread, NULL, writer_worker, bw))
        fwup_errx(EXIT_FAILURE, "pthread_create");
#endif

    return 0;
}

static ssize_t flush_buffer(struct block_writer *bw)
{
    assert(bw->buffer_index <= bw->buffer_size); // Math failed somewhere if we're buffering more than the block_size

    // Make sure that we're writing a whole block or pad it with 0s just in case.
    // Note: this isn't necessary when writing through a cache to a device, but that's
    //       not always the case.
    size_t block_boundary = (bw->buffer_index + ~bw->block_size_mask) & bw->block_size_mask;
    ssize_t added_bytes = bw->added_bytes;
    if (block_boundary != bw->buffer_index) {
        ssize_t padding_bytes = block_boundary - bw->buffer_index;
        memset(&bw->buffer[bw->buffer_index], 0, padding_bytes);
        added_bytes += padding_bytes;
        bw->buffer_index = block_boundary;
    }

    ssize_t rc = do_write(bw, bw->buffer, bw->buffer_index);
    if (rc != (ssize_t) bw->buffer_index)
        return -1;

    // Success. Adjust rc to not count the padding.
    bw->write_offset += bw->buffer_index;
    bw->buffer_index = 0;
    bw->added_bytes = 0;
    return rc - added_bytes;
}

/**
 * @brief flush and free the aligned writer data structures
 * @param bw
 * @return number of bytes written or -1 if error
 */
ssize_t block_writer_free(struct block_writer *bw)
{
    // Write any data that's still in the buffer
    ssize_t rc = 0;
    if (bw->buffer_index)
        rc = flush_buffer(bw);

#if USE_PTHREADS
    pthread_mutex_lock(&bw->mutex_back);
    bw->running = false;
    pthread_mutex_unlock(&bw->mutex_to);

    if (pthread_join(bw->writer_thread, NULL))
        fwup_errx(EXIT_FAILURE, "pthread_join");
    free_page_aligned(bw->async_buffer);
    pthread_mutex_destroy(&bw->mutex_to);
    pthread_mutex_destroy(&bw->mutex_back);
#endif

    // Free our buffer and clean up
    free_page_aligned(bw->buffer);
    bw->fd = -1;
    bw->buffer = 0;
    bw->buffer_index = 0;
    return rc;
}

/**
 * @brief Write a number of bytes to an offset like pwrite
 *
 * The offset must be monotonically increasing, but gaps are supported
 * when dealing with sparse files.
 *
 * @param bw
 * @param buf    the buffer
 * @param count  how many bytes to write
 * @param offset where to write in the file
 * @return -1 if error or the number of bytes written to Flash (if <count, then bytes were buffered)
 */
ssize_t block_writer_pwrite(struct block_writer *bw, const void *buf, size_t count, off_t offset)
{
    ssize_t amount_written = 0;
    if (bw->buffer_index) {
        // If we're buffering, fill in the buffer the rest of the way
        off_t position = bw->write_offset + bw->buffer_index;
        assert(offset >= position); // Check monotonicity of offset
        size_t to_write = bw->buffer_size - bw->buffer_index;
        if (offset > position) {
            off_t gap_size = offset - position;
            if (gap_size >= (off_t) to_write) {
                // Big gap, so flush the buffer
                ssize_t rc = flush_buffer(bw);
                if (rc < 0)
                    return rc;
                amount_written += rc;
                goto empty_buffer;
            } else {
                // Small gap, so fill it in with 0s and buffer more
                memset(&bw->buffer[bw->buffer_index], 0, gap_size);
                bw->added_bytes += gap_size;
                bw->buffer_index += gap_size;
                to_write -= gap_size;
            }
        }

        assert(offset == (off_t) (bw->write_offset + bw->buffer_index));
        if (count < to_write) {
            // Not enough to write to disk, so buffer for next time
            memcpy(&bw->buffer[bw->buffer_index], buf, count);
            bw->buffer_index += count;
            return amount_written;
        } else {
            // Fill out the buffer
            memcpy(&bw->buffer[bw->buffer_index], buf, to_write);
            buf += to_write;
            count -= to_write;
            offset += to_write;
            bw->buffer_index += to_write;
            ssize_t rc = flush_buffer(bw);
            if (rc < 0)
                return rc;
            amount_written += rc;
        }
    }

empty_buffer:
    assert(bw->buffer_index == 0);
    assert(offset >= bw->write_offset);
    assert(bw->added_bytes == 0);

    // Advance to the offset, but make sure that it's on a block boundary
    bw->write_offset = (offset & bw->block_size_mask);
    size_t padding = offset - bw->write_offset;
    if (padding > 0) {
        // NOTE: Padding both to the front and back seems like we could get into a situation
        //       where we overlap and wipe out some previously written data. This is impossible
        //       to do, though, since within a file, we're guaranteed to be monotonically
        //       increasing in our write offsets. Therefore, if we flush a block, we're guaranteed
        //       to never see that block again. Between files there could be a problem if the
        //       user configures overlapping blocks offsets between raw_writes. I can't think of
        //       a use case for this, but the config file format specifies everything in block
        //       offsets anyway.
        memset(bw->buffer, 0, padding);
        bw->added_bytes += padding;
        if (count + padding >= bw->block_size) {
            // Handle the block fragment
            size_t to_write = bw->block_size - padding;
            memcpy(&bw->buffer[padding], buf, to_write);
            if (do_write(bw, bw->buffer, bw->block_size) < 0)
                return -1;

            bw->write_offset += bw->block_size;
            amount_written += bw->block_size - bw->added_bytes;

            bw->added_bytes = 0;
            buf += to_write;
            count -= to_write;
        } else {
            // Buffer the block fragment for next time
            memcpy(&bw->buffer[padding], buf, count);
            bw->buffer_index = count + padding;
            return amount_written;
        }
    }

    // At this point, we're aligned to a block boundary
    assert((bw->write_offset & ~bw->block_size_mask) == 0);

    // If we have more than a buffer's worth, write all filled blocks
    // without even trying to buffer.
    while (count > bw->buffer_size) {
        size_t to_write = bw->buffer_size;
        memcpy(bw->buffer, buf, to_write); // copy to force alignment
        if (do_write(bw, bw->buffer, to_write) < 0)
            return -1;
        bw->write_offset += to_write;
        amount_written += to_write - bw->added_bytes;

        bw->added_bytes = 0;
        buf += to_write;
        count -= to_write;
    }

    // Buffer the left overs
    if (count) {
        memcpy(bw->buffer, buf, count);
        bw->buffer_index = count;
    }

    return amount_written;
}
