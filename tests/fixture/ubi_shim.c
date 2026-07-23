// LD_PRELOAD shim that emulates enough of the UBI volume char device
// update API for ubi_volume_write to run against a regular file:
//
//   1. UBI_IOCVOLUP puts the "volume" in update mode and records the
//      declared update size. (A real ioctl on a regular file would
//      fail with ENOTTY.)
//   2. write() is only allowed in update mode; otherwise EPERM.
//   3. pwrite() always fails with EPERM like a real UBI volume.
//
// Environment variables:
//   UBI_SHIM_CHECKPATH - path suffix of the fake UBI volume
//   UBI_SHIM_OUTFILE   - event log ("volup <declared>", "close <declared> <written>")
//
// UBI only exists on Linux, so this shim is a no-op elsewhere.

#ifdef __linux__

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <mtd/ubi-user.h>
#include "config.h"

#define ORIGINAL(name) original_##name
#define REPLACEMENT(name) name
#define OVERRIDE(ret, name, args) \
    static ret (*original_##name) args; \
    __attribute__((constructor)) void init_##name() { ORIGINAL(name) = dlsym(RTLD_NEXT, #name); } \
    ret REPLACEMENT(name) args

static int ubi_fd = -1;
static int updating = 0;
static int64_t declared_bytes = -1;
static int64_t written_bytes = 0;
static const char *ubi_pathname = "fake_ubi";

__attribute__((constructor)) void ubi_shim_init()
{
    char *override_path = getenv("UBI_SHIM_CHECKPATH");
    if (override_path)
        ubi_pathname = override_path;
}

static void log_event(const char *fmt, ...)
{
    const char *outfile = getenv("UBI_SHIM_OUTFILE");
    if (!outfile)
        return;

    FILE *f = fopen(outfile, "a");
    if (!f)
        return;

    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

static int match_ubi_path(const char *pathname)
{
    size_t ubi_pathname_len = strlen(ubi_pathname);
    size_t path_len = strlen(pathname);
    return path_len >= ubi_pathname_len &&
        strstr(pathname + path_len - ubi_pathname_len, ubi_pathname) != NULL;
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

    if (rc >= 0 && match_ubi_path(pathname)) {
        ubi_fd = rc;
        updating = 0;
        declared_bytes = -1;
        written_bytes = 0;
    }

    return rc;
}

OVERRIDE(int, open64, (const char *pathname, int flags, ...))
{
    int mode = 0;
    va_list ap;
    va_start(ap, flags);
    if (flags & O_CREAT)
        mode = va_arg(ap, int);
    va_end(ap);

    int rc = ORIGINAL(open64)(pathname, flags, mode);

    if (rc >= 0 && match_ubi_path(pathname)) {
        ubi_fd = rc;
        updating = 0;
        declared_bytes = -1;
        written_bytes = 0;
    }

    return rc;
}

OVERRIDE(int, ioctl, (int fd, unsigned long request, ...))
{
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);

    if (fd == ubi_fd && request == UBI_IOCVOLUP) {
        declared_bytes = *(const int64_t *) arg;
        written_bytes = 0;
        updating = 1;
        log_event("volup %lld\n", (long long) declared_bytes);
        return 0;
    }

    return ORIGINAL(ioctl)(fd, request, arg);
}

OVERRIDE(ssize_t, write, (int fd, const void *buf, size_t count))
{
    if (fd == ubi_fd) {
        if (!updating) {
            log_event("write-without-volup\n");
            errno = EPERM;
            return -1;
        }
        if (written_bytes + (int64_t) count > declared_bytes) {
            log_event("write-past-declared-size\n");
            errno = EFBIG;
            return -1;
        }
    }

    ssize_t rc = ORIGINAL(write)(fd, buf, count);
    if (fd == ubi_fd && rc > 0)
        written_bytes += rc;
    return rc;
}

// Real UBI volumes reject pwrite() with EPERM. If ubi_volume_write
// ever regresses to pwrite, the test will fail here.
OVERRIDE(ssize_t, pwrite, (int fd, const void *buf, size_t count, off_t offset))
{
    if (fd == ubi_fd) {
        log_event("pwrite-rejected\n");
        errno = EPERM;
        return -1;
    }
    return ORIGINAL(pwrite)(fd, buf, count, offset);
}

OVERRIDE(ssize_t, pwrite64, (int fd, const void *buf, size_t count, off_t offset))
{
    if (fd == ubi_fd) {
        log_event("pwrite-rejected\n");
        errno = EPERM;
        return -1;
    }
    return ORIGINAL(pwrite64)(fd, buf, count, offset);
}

OVERRIDE(int, close, (int fd))
{
    if (fd == ubi_fd) {
        log_event("close %lld %lld\n", (long long) declared_bytes, (long long) written_bytes);
        ubi_fd = -1;
        updating = 0;
    }
    return ORIGINAL(close)(fd);
}

#else

// Silence the empty-translation-unit warning on non-Linux platforms
int ubi_shim_not_supported;

#endif // __linux__
