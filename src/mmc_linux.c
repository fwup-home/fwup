/*
 * Copyright 2014-2017 Frank Hunleth
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
#include <sys/sysmacros.h>

#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/fs.h>
#include <linux/major.h>

#ifndef BLKDISCARD
#define BLKDISCARD _IO(0x12,119)
#endif

struct mmc_device_info
{
    char devpath[16];

    off_t device_size;
    struct stat st;
    bool removable;
};

static struct mmc_device_info mmc_devices[MMC_MAX_DEVICES];
static int mmc_device_count = -1;

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
 * @param name
 * @return
 */
static off_t mmc_device_size_sysfs(const char *name)
{
    char sysfspath[32];
    snprintf(sysfspath, sizeof(sysfspath), "/sys/block/%s/size", name);

    char sizestr[16];
    int rc = readsysfs(sysfspath, sizestr, sizeof(sizestr));
    if (rc <= 0)
        return 0;

    return strtoll(sizestr, NULL, 0) * FWUP_BLOCK_SIZE;
}

/**
 * @brief Return whether the device is removable
 * @param sysfspath
 * @return
 */
static bool mmc_device_removable_sysfs(const char *name)
{
    char sysfspath[32];
    snprintf(sysfspath, sizeof(sysfspath), "/sys/block/%s/removable", name);

    char sizestr[4];
    int rc = readsysfs(sysfspath, sizestr, sizeof(sizestr));
    if (rc <= 0)
        return false;

    return sizestr[0] == '1';
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

static bool mmc_get_device_stats(const char *name, struct mmc_device_info *info)
{
    snprintf(info->devpath, sizeof(info->devpath), "/dev/%s", name);

    if (stat(info->devpath, &info->st) < 0)
        return false;

    info->device_size = mmc_device_size_raw(info->devpath);
    if (info->device_size == 0)
        info->device_size = mmc_device_size_sysfs(name);

    // See https://elixir.bootlin.com/linux/v5.8.18/source/drivers/mmc/core/block.c#L2335 for
    // why mmc devices (e.g., built-in SDCard readers are marked non-removable). For fwup's
    // purposes, though, it makes more sense to count them as removable.
    info->removable = major(info->st.st_rdev) == MMC_BLOCK_MAJOR ||
        mmc_device_removable_sysfs(name);

    return true;
}

static bool is_autodetectable_mmc_device(const struct mmc_device_info *info, dev_t rootdev)
{
    // Check 1: Not on the device containing the root fs
    if (info->st.st_rdev == rootdev)
        return false;

    // Check 2: Zero capacity devices
    if (info->device_size <= 0)
        return false;

    // Check 3: Only allow removable drives
    if (!info->removable)
        return false;

    return true;
}

static void enumerate_mmc_devices()
{
    if (mmc_device_count >= 0)
        return;

    mmc_device_count = 0;

    // Scan memory cards connected via USB. These are /dev/sd_ devices.
    for (char c = 'a'; c != 'z' && mmc_device_count < MMC_MAX_DEVICES; c++) {
        char name[4];
        sprintf(name, "sd%c", c);
        if (mmc_get_device_stats(name, &mmc_devices[mmc_device_count]))
            mmc_device_count++;
    }

    // Scan the mmcblk devices
    for (int i = 0; i < 16 && mmc_device_count < MMC_MAX_DEVICES; i++) {
        char name[12];
        sprintf(name, "mmcblk%d", i);
        if (mmc_get_device_stats(name, &mmc_devices[mmc_device_count]))
            mmc_device_count++;
    }
}

static dev_t root_disk_device()
{
    // NOTE: We'd like to know the root device for the root directory, but stat
    //       only returns the partition device for the root directory. Since it
    //       is often the case of 16 minor devices to support each real device,
    //       assume that if we mask the root directory's device off that we'll
    //       get its parent.
    struct stat rootdev;
    if (stat("/", &rootdev) >= 0) {
        return (rootdev.st_dev & 0xfff0);
    } else {
        fwup_warnx("can't stat root directory");
        return 0;
    }
}

int mmc_scan_for_devices(struct mmc_device *devices, int max_devices)
{
    memset(devices, 0, max_devices * sizeof(struct mmc_device));

    enumerate_mmc_devices();

    // Get the root device so that we can filter
    // it's drive out of the autodetected list
    dev_t rootdev = root_disk_device();
    int device_count = 0;

    // Return everything that passes the is_autodetectable test.
    for (int i = 0; i < mmc_device_count && device_count < max_devices; i++) {
        if (is_autodetectable_mmc_device(&mmc_devices[i], rootdev)) {
            strcpy(devices[device_count].path, mmc_devices[i].devpath);
            devices[device_count].size = mmc_devices[i].device_size;
            device_count++;
        }
    }

    return device_count;
}

int mmc_device_size(const char *mmc_path, off_t *end_offset)
{
    // This function is called only when fwup has permission to
    // write to the device so the "raw" method should always work.
    *end_offset = mmc_device_size_raw(mmc_path);
    return *end_offset > 0 ? 0 : -1;
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

int mmc_is_path_at_device_offset(const char *file_path, off_t block_offset)
{
    struct stat file_st;
    if (stat(file_path, &file_st) < 0)
        return -1;

    // Now check that the offset is in the right place by reading the sysfs file.
    char start_path[64];
    snprintf(start_path, sizeof(start_path), "/sys/dev/block/%d:%d/start", major(file_st.st_dev), minor(file_st.st_dev));

    char start[16];
    if (readsysfs(start_path, start, sizeof(start)) <= 0)
        return -1;

    off_t start_offset = strtoull(start, NULL, 0);

    return start_offset == block_offset ? 1 : 0;
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
        fwup_exit(EXIT_FAILURE);
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

    char line[FWUP_BLOCK_SIZE] = {0};
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

int mmc_trim(int fd, off_t offset, off_t count)
{
    uint64_t range[2] = {offset, offset + count};

    if (ioctl(fd, BLKDISCARD, &range))
        fwup_warnx("BLKDISCARD (TRIM command) failed on range %"PRIu64" to %"PRIu64" (ignoring)", range[0], range[1]);

    return 0;
}


#endif // __linux__
