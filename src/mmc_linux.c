/*
 * Copyright 2014 LKC Technologies, Inc.
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

#ifdef __linux__

#define _GNU_SOURCE // for O_DIRECT

#include "mmc.h"
#include "util.h"

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int readsysfs(const char *path, char *buffer, int maxlen)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;

    int count = read(fd, buffer, maxlen - 1);

    close(fd);
    if (count <= 0)
        return 0;

    // Trim trailing \n
    count--;
    buffer[count] = 0;
    return count;
}

/**
 * @brief Return the device size by using the sysfs
 * @param sysfspath
 * @return
 */
static off_t mmc_device_size_sysfs(const char *sysfspath)
{
    char sizestr[16];
    int rc = readsysfs(sysfspath, sizestr, sizeof(sizestr));
    if (rc <= 0)
        return 0;

    return strtoll(sizestr, NULL, 0) * 512;
}

/**
 * @brief Return the device size by seeking to the end (requires root permissions)
 * @param devpath device path
 * @return >0 if it worked, 0 if it didn't
 */
static off_t mmc_device_size_raw(const char *devpath)
{
    int fd = open(devpath, O_RDONLY);
    if (fd < 0)
        return 0;

    // fstat will not return the mmc size on Linux, so use lseek
    // instead.
    off_t len = lseek(fd, 0, SEEK_END);
    close(fd);

    // Treat errors and 0-length device sizes the same
    return len < 0 ? 0 : len;
}

struct mmc_device_info
{
    char devpath[16];

    off_t device_size;
    struct stat st;
};

static bool mmc_get_device_stats(const char *devpath_pattern,
                                 const char *sysfs_size_pattern,
                                 int instance,
                                 struct mmc_device_info *info)
{
    snprintf(info->devpath, sizeof(info->devpath), devpath_pattern, instance);

    if (stat(info->devpath, &info->st) < 0)
        return false;

    info->device_size = mmc_device_size_raw(info->devpath);
    if (info->device_size == 0) {
        char sysfspath[32];
        snprintf(sysfspath, sizeof(sysfspath), sysfs_size_pattern, instance);
        info->device_size = mmc_device_size_sysfs(sysfspath);
    }
    return true;
}

static bool is_autodetectable_mmc_device(const struct mmc_device_info *info, const struct stat *rootdev)
{
    // Check 1: Not on the device containing the root fs
    // NOTE: This check is an approximation to what we really want. We'd like
    //       to know the root device for the root directory, but stat only
    //       returns the partition device for the root directory. Since it
    //       is often the case of 16 minor devices to support each real device,
    //       assume that if we mask the root directory's device off that we'll get
    //       its parent.
    if (info->st.st_rdev == (rootdev->st_dev & 0xfff0))
        return false;

    // Check 2: Zero capacity devices
    if (info->device_size <= 0)
        return false;

    // Check 3: Capacity larger than 65 GiB -> false
    // NOTE: The rationale for this check is that the user's main drives will be
    //       large capacity and we don't want to autodetect them when looking for
    //       SDCards.
    if (info->device_size > (65 * ONE_GiB))
        return false;

    return true;
}

int mmc_scan_for_devices(struct mmc_device *devices, int max_devices)
{
    // Get the root device so that we can filter
    // it's drive out of the autodetected list
    struct stat rootdev;
    if (stat("/", &rootdev) < 0) {
        fwup_warnx("can't stat root directory");
        rootdev.st_dev = 0;
    }

    int device_count = 0;

    // Scan memory cards connected via USB. These are /dev/sd_ devices.
    for (char c = 'a'; c != 'z'; c++) {
        struct mmc_device_info info;
        if (mmc_get_device_stats("/dev/sd%c",
                                 "/sys/block/sd%c/size",
                                 c,
                                 &info) &&
            is_autodetectable_mmc_device(&info, &rootdev) &&
            device_count < max_devices) {
            strcpy(devices[device_count].path, info.devpath);
            devices[device_count].size = info.device_size;
            device_count++;
        }
    }

    // Scan the mmcblk devices
    for (int i = 0; i < 16; i++) {
        struct mmc_device_info info;
        if (mmc_get_device_stats("/dev/mmcblk%d",
                                 "/sys/block/mmcblk%d/size",
                                 i,
                                 &info) &&
            is_autodetectable_mmc_device(&info, &rootdev) &&
            device_count < max_devices) {
            strcpy(devices[device_count].path, info.devpath);
            devices[device_count].size = info.device_size;
            device_count++;
        }
    }

    return device_count;
}

int mmc_is_path_on_device(const char *file_path, const char *device_path)
{
    // Stat both paths.
    struct stat file_st;
    if (stat(file_path, &file_st) < 0)
        return -1;

    struct stat device_st;
    if (stat(device_path, &device_st) < 0)
        return -1;

    // Check that the device's major/minor are the same
    // as the file's containing device's major/minor
    return device_st.st_rdev == file_st.st_dev ? 1 : 0;
}

static char *unescape_string(const char *input)
{
    char *result = (char *) malloc(strlen(input) + 1);
    char *p = result;
    while (*input) {
        if (*input != '\\') {
            *p = *input;
            p++;
            input++;
        } else {
            input++;
            switch (*input) {
            case '\"':
            case '\\':
            default:
                *p = *input++;
                break;
            case 'a':
                *p = '\a'; input++;
                break;
            case 'b':
                *p = '\b'; input++;
                break;
            case 'f':
                *p = '\f'; input++;
                break;
            case 'n':
                *p = '\n'; input++;
                break;
            case 'r':
                *p = '\r'; input++;
                break;
            case 't':
                *p = '\t'; input++;
                break;
            case 'v':
                *p = '\v'; input++;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7': // octal
            {
                int digits = (input[1] && input[1] >= '0' && input[1] <= '7' ? 1 : 0)
                             + (input[1] && input[2] && input[2] >= '0' && input[2] <= '7' ? 1 : 0);
                int result = *input++ - '0';
                while (digits--)
                    result = result * 8 + *input++ - '0';
                *p = (char) result;
                break;
            }
            }
            p++;
        }
    }
    *p = 0;
    return result;
}

static int fork_exec(const char *path, const char *arg)
{
    pid_t pid = fork();
    if (pid == 0) {
        // child
        execl(path, path, arg, NULL);

        // Not supposed to reach here.
        exit(EXIT_FAILURE);
    } else {
        // parent
        int status;
        if (waitpid(pid, &status, 0) != pid)
            return -1;

        return status;
    }
}

int mmc_umount_all(const char *mmc_device)
{
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp)
        fwup_err(EXIT_FAILURE, "/proc/mounts");

    char *todo[64] = {0};
    int todo_ix = 0;
    int ultimate_rc = 0;

    char line[512] = {0};
    while (!feof(fp) &&
            fgets(line, sizeof(line), fp)) {
        char devname[64];
        char mountpoint[256];
        if (sscanf(line, "%63s %255s", devname, mountpoint) != 2)
            continue;

        if (strstr(devname, mmc_device) == devname) {
            // mmc_device is a prefix of this device, i.e. mmc_device is /dev/sdc
            // and /dev/sdc1 is mounted.

            if (todo_ix == NUM_ELEMENTS(todo))
                fwup_errx(EXIT_FAILURE, "Device mounted too many times");

            // strings from /proc/mounts are escaped, so unescape them
            todo[todo_ix++] = unescape_string(mountpoint);
        }
    }
    fclose(fp);

    int mtab_exists = (access("/etc/mtab", F_OK) != -1);
    for (int i = 0; i < todo_ix; i++) {
        if (mtab_exists) {
            // If /etc/mtab, then call umount(8) so that
            // gets updated correctly.
            int rc = fork_exec("/bin/umount", todo[i]);
            if (rc != 0) {
                fwup_warnx("Error calling umount on '%s'", todo[i]);
                ultimate_rc = -1;
            }
        } else {
            // No /etc/mtab, so call the kernel directly.
#if HAS_UMOUNT
            if (umount(todo[i]) < 0) {
                fwup_warnx("umount %s", todo[i]);
                ultimate_rc = -1;
            }
#else
            // If no umount on this platform, warn, but don't
            // return failure.
            fwup_warnx("umount %s: not supported", todo[i]);
#endif
        }
    }

    for (int i = 0; i < todo_ix; i++)
        free(todo[i]);

    return ultimate_rc;
}

int mmc_eject(const char *mmc_device)
{
    // Linux doesn't complain if you don't eject
    (void) mmc_device;
    return 0;
}

int mmc_open(const char *mmc_path)
{
    return open(mmc_path, O_RDWR | O_DIRECT);
}

void mmc_init()
{
}

void mmc_finalize()
{
}

#endif // __linux__
