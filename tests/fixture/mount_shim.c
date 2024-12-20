// This shim simulates various filesystem mounting scenarios for testing purposes.
//
// It intercepts stat() calls to set device numbers and redirects calls going to
// /sys to under the $WORK directory.

#define _GNU_SOURCE // for RTLD_NEXT
#include <dirent.h>
#include <dlfcn.h>
#include <err.h>
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

#ifdef __APPLE__
#define OVERRIDE_STAT 1
#else
#if __GLIBC_PREREQ(2, 33)
#define OVERRIDE_STAT 1
#else
#define OVERRIDE_XSTAT 1
#endif
#endif

static const char *work = NULL;

__attribute__((constructor)) void mount_shim_init()
{
    work = getenv("WORK");
    if (work == NULL)
        work = ".";
}

static void fixup_path(const char *input, char *output)
{
    // If looking under /sys, move to work directory for fake files.
    if (strncmp(input, "/sys/", 5) == 0)
        sprintf(output, "%s%s", work, input);
    else
        strcpy(output, input);
}

#ifdef __USE_LARGEFILE64
OVERRIDE(FILE *, fopen64, (const char *pathname, const char *mode))
{
    char new_path[PATH_MAX];
    fixup_path(pathname, new_path);
    return ORIGINAL(fopen64)(new_path, mode);
}
#else
OVERRIDE(FILE *, fopen, (const char *pathname, const char *mode))
{
    char new_path[PATH_MAX];
    fixup_path(pathname, new_path);
    return ORIGINAL(fopen)(new_path, mode);
}
#endif

#ifdef __USE_LARGEFILE64
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

    char new_path[PATH_MAX];
    fixup_path(pathname, new_path);

    return ORIGINAL(open64)(new_path, flags, mode);
}
#else
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

    char new_path[PATH_MAX];
    fixup_path(pathname, new_path);

    return ORIGINAL(open)(new_path, flags, mode);
}
#endif

#ifdef __USE_LARGEFILE64
OVERRIDE(int, scandir64, (const char *dirp, struct dirent64 ***namelist, int (*filter)(const struct dirent64 *), int (*compar)(const struct dirent64 **, const struct dirent64 **)))
{
    char new_path[PATH_MAX];
    fixup_path(dirp, new_path);
    return ORIGINAL(scandir64)(new_path, namelist, filter, compar);
}
#else
OVERRIDE(int, scandir, (const char *dirp, struct dirent ***namelist, int (*filter)(const struct dirent *), int (*compar)(const struct dirent **, const struct dirent **)))
{
    char new_path[PATH_MAX];
    fixup_path(dirp, new_path);

    return ORIGINAL(scandir)(new_path, namelist, filter, compar);
}
#endif

static int stat_impl(const char *pathname, mode_t *mode, dev_t *dev, dev_t *rdev)
{
    if (strcmp(pathname, "/") == 0) {
        *dev = 0xb301;
        *mode = S_IFDIR;
        return 0;
    } else if (strcmp(pathname, "/boot") == 0) {
        *dev = 0xfe00;
        *mode = S_IFDIR;
        return 0;
    } else if (strcmp(pathname, "/dev/mmcblk0") == 0) {
        *rdev = 0xb300;
        *mode = S_IFBLK;
        return 0;
    } else if (strcmp(pathname, "/dev/mmcblk0p1") == 0) {
        *rdev = 0xb301;
        *mode = S_IFBLK;
        return 0;
    } else if (strcmp(pathname, "/dev/mmcblk0p2") == 0) {
        *rdev = 0xb302;
        *mode = S_IFBLK;
        return 0;
    } else if (strcmp(pathname, "/dev/mmcblk0p3") == 0) {
        *rdev = 0xb303;
        *mode = S_IFBLK;
        return 0;
    } else if (strcmp(pathname, "/dev/mmcblk0p4") == 0) {
        *rdev = 0xb304;
        *mode = S_IFBLK;
        return 0;
    } else if (strcmp(pathname, "/dev/rootdisk0p1") == 0) {
        *rdev = 0xb301;
        *mode = S_IFBLK;
        return 0;
    } else if (strcmp(pathname, "/dev/dm-0") == 0) {
        *rdev = 0xfe00;
        *mode = S_IFBLK;
        return 0;
    } else if (strcmp(pathname, "/dev/dm-1") == 0) {
        *rdev = 0xfe01;
        *mode = S_IFBLK;
        return 0;
     } else {
        return -1;
    }
}

#ifdef OVERRIDE_STAT
#ifdef __USE_LARGEFILE64
OVERRIDE(int, stat64, (const char *pathname, struct stat64 *st))
{
    memset(st, 0, sizeof(*st));
    if (stat_impl(pathname, &st->st_mode, &st->st_dev, &st->st_rdev) == 0)
        return 0;
    else
        return ORIGINAL(stat64)(pathname, st);
}
#else
OVERRIDE(int, stat, (const char *pathname, struct stat *st))
{
    memset(st, 0, sizeof(*st));
    if (stat_impl(pathname, &st->st_mode, &st->st_dev, &st->st_rdev) == 0)
        return 0;
    else
        return ORIGINAL(stat)(pathname, st);
}
#endif
#endif
#ifdef OVERRIDE_XSTAT
#ifdef __USE_LARGEFILE64
OVERRIDE(int, __xstat64, (int ver, const char *pathname, struct stat64 *st))
{
    memset(st, 0, sizeof(*st));
    if (stat_impl(pathname, &st->st_mode, &st->st_dev, &st->st_rdev) == 0)
        return 0;
    else
        return ORIGINAL(__xstat64)(ver, pathname, st);
}
#endif
#endif
