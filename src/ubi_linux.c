/*
 * Copyright 2026 Herman verschooten
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ubi.h"
#include "util.h"

#ifdef __linux__

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <mtd/ubi-user.h>

int ubi_volume_update_start(const char *path, int64_t total_bytes)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0)
        ERR_RETURN("ubi_volume_write can't open '%s': %s", path, strerror(errno));

    if (ioctl(fd, UBI_IOCVOLUP, &total_bytes) < 0) {
        close(fd);
        ERR_RETURN("UBI_IOCVOLUP failed on '%s': %s. Check that it's a UBI volume (see /sys/class/ubi)",
                   path, strerror(errno));
    }

    return fd;
}

int ubi_volume_update_write(const char *path, int fd, const void *buf, size_t count)
{
    const char *p = buf;
    while (count > 0) {
        ssize_t n = write(fd, p, count);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            ERR_RETURN("write to '%s' failed: %s", path, strerror(errno));
        }
        p += n;
        count -= n;
    }
    return 0;
}

int ubi_volume_update_finish(const char *path, int fd)
{
    if (close(fd) < 0)
        ERR_RETURN("close('%s') failed: %s", path, strerror(errno));
    return 0;
}

#else

int ubi_volume_update_start(const char *path, int64_t total_bytes)
{
    (void) path;
    (void) total_bytes;
    ERR_RETURN("ubi_volume_write is only supported on Linux");
}

int ubi_volume_update_write(const char *path, int fd, const void *buf, size_t count)
{
    (void) path;
    (void) fd;
    (void) buf;
    (void) count;
    ERR_RETURN("ubi_volume_write is only supported on Linux");
}

int ubi_volume_update_finish(const char *path, int fd)
{
    (void) path;
    (void) fd;
    ERR_RETURN("ubi_volume_write is only supported on Linux");
}

#endif // __linux__
