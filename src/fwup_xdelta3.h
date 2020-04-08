#ifndef FWUP_XDELTA3_H
#define FWUP_XDELTA3_H

#include <stdio.h>
#include <stdbool.h>

// Force XD3 defines to avoid compiling extra code.
#define XD3_ENCODER 0
#define XD3_DEBUG 0
#define SECONDARY_FGK 0
#define SECONDARY_DJW 0
#define SECONDARY_LZMA 0

#include "config.h"
#include "3rdparty/xdelta3/xdelta3.h"

// The patch read block interface matches the zero-copy API of libarchive
// for convenience, and not as an optimization.
typedef int (xdelta_read_patch_block)(void *cookie, const void **buffer, size_t *count);
typedef int (xdelta_pread_source)(void *cookie, void *buffer, size_t count, off_t offset);

struct xdelta_state {
    xd3_stream stream;
    xd3_source source;

    xdelta_read_patch_block *read_patch;
    xdelta_pread_source *pread_source;
    void *cookie;
    bool end_of_patch;
};

void xdelta_init(struct xdelta_state *xd, xdelta_read_patch_block *read_patch, xdelta_pread_source *pread_source, void *cookie);
int xdelta_read(struct xdelta_state *xd, const void **buffer, size_t *count);
int xdelta_read_header(struct xdelta_state *xd);
void xdelta_free(struct xdelta_state *xd);

#endif
