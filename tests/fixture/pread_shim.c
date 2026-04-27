#define _GNU_SOURCE
#include <err.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "config.h"

#ifndef __APPLE__
#define ORIGINAL(name) original_##name
#define REPLACEMENT(name) name
#define OVERRIDE(ret, name, args) \
    static ret (*original_##name) args; \
    __attribute__((constructor)) void init_##name() { ORIGINAL(name) = dlsym(RTLD_NEXT, #name); } \
    ret REPLACEMENT(name) args
#else
#define ORIGINAL(name) name
#define REPLACEMENT(name) new_##name
#define OVERRIDE(ret, name, args) \
    ret REPLACEMENT(name) args; \
    __attribute__((used)) static struct { const void *original; const void *replacement; } _interpose_##name \
    __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&REPLACEMENT(name), (const void*)(unsigned long)&ORIGINAL(name) }; \
    ret REPLACEMENT(name) args
#endif

static int image_fd = -1;
static int pread_count = 0;
static const char *image_pathname = "fwup.img";

__attribute__((constructor)) void pread_shim_init()
{
    char *override_path = getenv("PREAD_SHIM_CHECKPATH");
    if (override_path)
        image_pathname = override_path;
}

__attribute__((destructor)) void pread_shim_fini()
{
    const char *outfile = getenv("PREAD_SHIM_OUTFILE");
    if (outfile) {
        FILE *f = fopen(outfile, "w");
        if (f) {
            fprintf(f, "%d\n", pread_count);
            fclose(f);
        }
    }
}

OVERRIDE(int, open, (const char *pathname, int flags, ...))
{
    int mode = 0;
    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT)
        mode = va_arg(ap, int);
    va_end(ap);

    int rc = ORIGINAL(open)(pathname, flags, mode);

    if (rc >= 0) {
        size_t image_pathname_len = strlen(image_pathname);
        size_t path_len = strlen(pathname);
        if (path_len >= image_pathname_len &&
                strstr(pathname + path_len - image_pathname_len, image_pathname) != NULL)
            image_fd = rc;
    }

    return rc;
}

#ifndef __APPLE__
OVERRIDE(int, open64, (const char *pathname, int flags, ...))
{
    int mode = 0;
    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT)
        mode = va_arg(ap, int);
    va_end(ap);

    int rc = ORIGINAL(open64)(pathname, flags, mode);

    if (rc >= 0) {
        size_t image_pathname_len = strlen(image_pathname);
        size_t path_len = strlen(pathname);
        if (path_len >= image_pathname_len &&
                strstr(pathname + path_len - image_pathname_len, image_pathname) != NULL)
            image_fd = rc;
    }

    return rc;
}
#endif

OVERRIDE(ssize_t, pread, (int fd, void *buf, size_t nbyte, off_t offset))
{
    if (fd == image_fd)
        pread_count++;
    return ORIGINAL(pread)(fd, buf, nbyte, offset);
}

#ifndef __APPLE__
OVERRIDE(ssize_t, pread64, (int fd, void *buf, size_t nbyte, off_t offset))
{
    if (fd == image_fd)
        pread_count++;
    return ORIGINAL(pread64)(fd, buf, nbyte, offset);
}
#endif
