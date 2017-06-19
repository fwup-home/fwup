#include "block_cache.h"

#include <string.h>
#include <unistd.h>


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

    return 0;
}

int block_cache_flush(struct block_cache *bc)
{
#if 0
    ssize_t lastwritten = block_writer_free(&bc->writer);
    if (lastwritten < 0)
        ERR_CLEANUP_MSG("couldn't flush cache");


cleanup:
    return -1;
#endif
    return 0;
}

/**
 * @brief block_cache_free
 * @param bc
 * @return
 */
int block_cache_free(struct block_cache *bc)
{
    int rc = 0;
    ssize_t lastwritten = block_writer_free(&bc->writer);
    if (lastwritten < 0)
        ERR_CLEANUP_MSG("couldn't flush cache");

cleanup:
    return rc;
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
ssize_t block_cache_clear_valid(struct block_cache *bc, off_t offset, size_t count)
{

    return count;
}

int block_cache_pwrite(struct block_cache *bc, const void *buf, size_t count, off_t offset, bool streamed)
{
    if (pwrite(bc->fd, buf, count, offset) != count)
        ERR_RETURN("pwrite");

    return 0;
}

int block_cache_pread(struct block_cache *bc, void *buf, size_t count, off_t offset)
{
    if (pread(bc->fd, buf, count, offset) != count)
        ERR_RETURN("pread");

    return 0;
}
