#define _GNU_SOURCE // for RTLD_NEXT
#include <err.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"

#ifndef __APPLE__
#define ORIGINAL(name) original_##name
#define REPLACEMENT(name) name
#define OVERRIDE(ret, name, args) \
    static ret (*original_##name) args; \
    __attribute__((constructor)) void init_##name() { ORIGINAL(name) = dlsym(RTLD_NEXT, #name); } \
    ret REPLACEMENT(name) args

#define REPLACE(ret, name, args) \
    ret REPLACEMENT(name) args
#else
#define ORIGINAL(name) name
#define REPLACEMENT(name) new_##name
#define OVERRIDE(ret, name, args) \
    ret REPLACEMENT(name) args; \
    __attribute__((used)) static struct { const void *original; const void *replacement; } _interpose_##name \
    __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&REPLACEMENT(name), (const void*)(unsigned long)&ORIGINAL(name) }; \
    ret REPLACEMENT(name) args

#define REPLACE(ret, name, args) OVERRIDE(ret, name, args)
#endif

static int write_fd = -1;
static off_t write_offset = -1;
static const char *write_pathname = "fwup.img";
static off_t corrupt_write_offset = -1;

#if HAVE_SYSCONF
static size_t cached_pagesize = 0;
#else
// If no sysconf(), guess the page size
static const size_t cached_pagesize = 4096;
#endif

static inline size_t get_pagesize()
{
#if HAVE_SYSCONF
    // If sysconf() exists, then call it to find the system's page size
    if (cached_pagesize == 0) {
        long rc = sysconf(_SC_PAGESIZE);
        if (rc > 0)
            cached_pagesize = rc;
        else
            cached_pagesize = 4096; // Guess
    }
#endif
    return cached_pagesize;
}

void alloc_page_aligned(void **memptr, size_t size)
{
    size_t pagesize = get_pagesize();

#if HAVE_POSIX_MEMALIGN
    if (posix_memalign(memptr, pagesize, size) < 0)
        err(EXIT_FAILURE, "posix_memalign %u bytes", (unsigned int) size);
#else
    // Slightly wasteful implementation of posix_memalign
    size_t padding = pagesize + pagesize - 1;
    uint8_t *original = (uint8_t *) malloc(size + padding);
    if (original == NULL)
        err(EXIT_FAILURE, "malloc %d bytes", (int) (size + padding));

    // Store the original pointer right before the aligned pointer
    uint8_t *aligned = (uint8_t *) (((uint64_t) (original + padding)) & ~(pagesize - 1));
    void **savelocation = (void**) (aligned - sizeof(void*));
    *savelocation = original;
    *memptr = aligned;
#endif
}

void free_page_aligned(void *memptr)
{
#if HAVE_POSIX_MEMALIGN
    free(memptr);
#else
    void **savelocation = ((void **) memptr) - 1;
    void *original = *savelocation;
    free(original);
#endif
}

__attribute__((constructor)) void write_shim_init()
{
    char *override_path = getenv("WRITE_SHIM_CHECKPATH");
    if (override_path)
        write_pathname = override_path;

    char *corrupt_write_offset_str = getenv("WRITE_SHIM_CORRUPT_OFFSET");
    if (corrupt_write_offset_str)
        corrupt_write_offset = strtoll(corrupt_write_offset_str, NULL, 0);
}

OVERRIDE(int, open, (const char *pathname, int flags, ...))
{
    int mode;

    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT)
        mode = va_arg(ap, int);
    else
        mode = 0;
    va_end(ap);

    int rc = ORIGINAL(open)(pathname, flags, mode);

    // Did the call to open succeed?
    if (rc >= 0) {
        // Is this the file of interest (match on suffix)?
        size_t write_pathname_len = strlen(write_pathname);
        size_t path_len = strlen(pathname);
        if (path_len >= write_pathname_len &&
                strstr(pathname + path_len - write_pathname_len, write_pathname) != 0) {
            if (write_fd >= 0)
                errx(EXIT_FAILURE, "Opening %s twice is not supported", pathname);
            write_fd = rc;
            write_offset = 0;

            // Call the aligned allocation routines to verify that they work.
            void *testing;
            alloc_page_aligned(&testing, 128*1024);
            free_page_aligned(testing);
        }
    }

    return rc;
}

#ifndef __APPLE__
OVERRIDE(int, open64, (const char *pathname, int flags, ...))
{
    int mode;

    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT)
        mode = va_arg(ap, int);
    else
        mode = 0;
    va_end(ap);

    int rc = ORIGINAL(open64)(pathname, flags, mode);

    // Did the call to open succeed?
    if (rc >= 0) {
        // Is this the file of interest (match on suffix)?
        size_t write_pathname_len = strlen(write_pathname);
        size_t path_len = strlen(pathname);
        if (path_len >= write_pathname_len &&
                strstr(pathname + path_len - write_pathname_len, write_pathname) != 0) {
            if (write_fd >= 0)
                errx(EXIT_FAILURE, "Opening %s twice is not supported", pathname);
            write_fd = rc;
            write_offset = 0;

             // Call the aligned allocation routines to verify that they work.
            void *testing;
            alloc_page_aligned(&testing, 128*1024);
            free_page_aligned(testing);
        }
    }

    return rc;
}
#endif

static void corrupt_buffer(void *buf, size_t offset)
{
    uint8_t *cbuf = buf;
    cbuf[offset] ^= 0x1;
}

OVERRIDE(ssize_t, pwrite, (int fd, const void *buf, size_t nbyte, off_t offset))
{
    if (fd == write_fd) {
        off_t last_offset = offset + nbyte;
        if (corrupt_write_offset >= offset && corrupt_write_offset < last_offset) {
            size_t offset_in_buffer = corrupt_write_offset - offset;

            void *new_buf;
            alloc_page_aligned(&new_buf, nbyte);
            memcpy(new_buf, buf, nbyte);

            corrupt_buffer(new_buf, offset_in_buffer);

            ssize_t rc = ORIGINAL(pwrite)(fd, new_buf, nbyte, offset);
            free_page_aligned(new_buf);
            return rc;
        }
    }

    return ORIGINAL(pwrite)(fd, buf, nbyte, offset);
}

#ifndef __APPLE__
OVERRIDE(ssize_t, pwrite64, (int fd, const void *buf, size_t nbyte, off_t offset))
{
    if (fd == write_fd) {
        off_t last_offset = offset + nbyte;
        if (corrupt_write_offset >= offset && corrupt_write_offset < last_offset) {
            size_t offset_in_buffer = corrupt_write_offset - offset;

            void *new_buf;
            alloc_page_aligned(&new_buf, nbyte);
            memcpy(new_buf, buf, nbyte);

            corrupt_buffer(new_buf, offset_in_buffer);

            ssize_t rc = ORIGINAL(pwrite64)(fd, new_buf, nbyte, offset);
            free_page_aligned(new_buf);
            return rc;
        }
    }

    return ORIGINAL(pwrite64)(fd, buf, nbyte, offset);
}
#endif
