#include "fwup_xdelta3.h"
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include "3rdparty/xdelta3/xdelta3.c"
#include "util.h"

#define READ_SIZE (128 * 1024)

void xdelta_init(struct xdelta_state *xd, xdelta_read_patch_block *read_patch, xdelta_pread_source *pread_source, void *cookie)
{
    memset(xd, 0, sizeof(*xd));

    xd3_config config;
    xd3_init_config(&config, XD3_ADLER32);
    xd3_config_stream(&xd->stream, &config);

    xd->source.blksize = READ_SIZE;
    xd->source.curblk = malloc(READ_SIZE);

    xd->read_patch = read_patch;
    xd->pread_source = pread_source;
    xd->cookie = cookie;
    xd->end_of_patch = false;
}

void xdelta_free(struct xdelta_state *xd)
{
    free((void*) xd->source.curblk);
    xd->source.curblk = 0;

    xd3_close_stream(&xd->stream);
    xd3_free_stream(&xd->stream);
}

static void xdelta_consume(struct xdelta_state *xd, const void **buffer, size_t *count)
{
    *buffer = xd->stream.next_out;
    *count = xd->stream.avail_out;

    // Output buffer is invalidated on next call to xdelta_read.
    xd3_consume_output(&xd->stream);
}

static int xdelta_read_more(struct xdelta_state *xd)
{
    if (xd->end_of_patch)
        return 0;

    const void *buffer;
    size_t len;
    if (xd->read_patch(xd->cookie, &buffer, &len) >= 0) {
        xd3_avail_input(&xd->stream, buffer, len);
        if (len == 0) {
            xd3_set_flags(&xd->stream, XD3_FLUSH | xd->stream.flags);
            xd->end_of_patch = true;
        }
        return 1;
    } else {
        return -1;
    }
}

static int xdelta_read_source_block(struct xdelta_state *xd, xoff_t blkno)
{
    int rc = xd->pread_source(xd->cookie,
                              (void *) xd->source.curblk,
                              xd->source.blksize,
                              xd->source.blksize * blkno);
    if (rc < 0)
        return -1;

    xd->source.onblk = rc;
    xd->source.curblkno = blkno;

    return 1;
}

// Returns 0 when done; >0 when more to do; <0 on error
static int xdelta_read_impl(struct xdelta_state *xd, const void **buffer, size_t *count)
{
    switch (xd3_decode_input(&xd->stream)) {
    case XD3_INPUT:
        return xdelta_read_more(xd);

    case XD3_GOTHEADER:
        if (xdelta_read_source_block(xd, 0) > 0) {
            xd3_set_source(&xd->stream, &xd->source);
            return 1;
        } else {
            return -1;
        }

    case XD3_WINSTART:
    case XD3_WINFINISH:
        return 1;

    case XD3_GETSRCBLK:
        return xdelta_read_source_block(xd, xd->source.getblkno);

    case XD3_OUTPUT:
        xdelta_consume(xd, buffer, count);
        return 0;

    default:
        ERR_RETURN("xdelta3 error: %s", xd->stream.msg);
    }
}

/**
 * Read from an xdelta-encoded source
 *
 * @param buffer - a pointer to the buffer read is returned (zero copy)
 * @param count - the number of bytes is returned
 * @returns 0 on success; <0 on error
 */
int xdelta_read(struct xdelta_state *xd, const void **buffer, size_t *count)
{
    *count = 0;

    int rc;
    while ((rc = xdelta_read_impl(xd, buffer, count)) > 0);

    return rc;
}

// Returns 0 when done; >0 when more to do; <0 on error
static int xdelta_read_header_impl(struct xdelta_state *xd)
{
    switch (xd3_decode_input(&xd->stream)) {
    case XD3_INPUT:
        return xdelta_read_more(xd);

    case XD3_GOTHEADER:
        return 0;

    case XD3_WINSTART:
    case XD3_WINFINISH:
        return 1;

    case XD3_GETSRCBLK:
        return -1;

    case XD3_OUTPUT:
        return -1;

    default:
        ERR_RETURN("xdelta3 error: %s", xd->stream.msg);
    }
}

/**
 * Only read the xdelta3 header
 *
 * @param buffer - a pointer to the buffer read is returned (zero copy)
 * @param count - the number of bytes is returned
 * @returns 0 on success; <0 on error
 */
int xdelta_read_header(struct xdelta_state *xd)
{
    int rc;
    while ((rc = xdelta_read_header_impl(xd)) > 0);

    return rc;
}
