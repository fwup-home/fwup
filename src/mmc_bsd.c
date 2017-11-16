/*
 * Copyright 2016-2017 Frank Hunleth
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

#if defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)

#include "mmc.h"
#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>

struct mmc_device_info
{
    char devpath[16];

    off_t device_size;
    struct stat st;
};

static bool mmc_get_device_stats(const char *devpath_pattern,
                                 int instance,
                                 struct mmc_device_info *info)
{
    snprintf(info->devpath, sizeof(info->devpath), devpath_pattern, instance);

    if (stat(info->devpath, &info->st) < 0)
        return false;

    int fd = open(info->devpath, O_RDONLY);
    if (fd < 0)
        return false;

    // fstat will not return the mmc size on Linux, so use lseek
    // instead.
    info->device_size = lseek(fd, 0, SEEK_END);
    close(fd);

    // Treat errors and 0-length device sizes the same
    if (info->device_size < 0)
	info->device_size = 0;

    return true;
}

static bool is_autodetectable_mmc_device(const struct mmc_device_info *info, 
                                         const struct stat *rootdev)
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

static void mmc_scan_by_pattern(struct mmc_device *devices,
                                const char *pattern,
                                int low,
                                int high,
                                const struct stat *rootdev,
                                int max_devices,
                                int *device_count)
{
    // Scan memory cards connected via USB.
    int dc = *device_count;
    for (int i = low; i <= high; i++) {
        struct mmc_device_info info;
        if (mmc_get_device_stats(pattern, i, &info) &&
            is_autodetectable_mmc_device(&info, rootdev) &&
            *device_count < max_devices) {
            snprintf(devices[dc].path, sizeof(devices[dc].path), "%s", info.devpath);
            devices[dc].size = info.device_size;
            dc++;
        }
    }
    *device_count = dc;
}

/**
 * @brief Scan for SDCards and other removable media
 * @param devices where to store detected devices and some metadata
 * @param max_devices the max to return
 * @return the number of devices found
 */
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

    // Scan memory cards connected via USB.
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    mmc_scan_by_pattern(devices, "/dev/da%d", 0, 15, &rootdev, max_devices, &device_count);
#elif defined(__NetBSD__)
    mmc_scan_by_pattern(devices, "/dev/rs%c0d", 'a', 'z', &rootdev, max_devices, &device_count);
#endif

    return device_count;
}

int mmc_find_root_drive(struct mmc_device *device)
{
    fwup_warnx("_fwup_root_disk and the -Z option not implemented on the BSDs");
    return -1;
}

int mmc_umount_all(const char *mmc_device)
{
    fwup_warnx("umount %s not implemented. Pass -U to avoid warning.", mmc_device);
    return 0;
}

int mmc_eject(const char *mmc_device)
{
    // FreeBSD doesn't complain if you don't eject
    (void) mmc_device;
    return 0;
}

/**
 * @brief Open an SDCard/MMC device
 * @param mmc_path the path
 * @return a filehandle or <0 on error
 */
int mmc_open(const char *mmc_path)
{
    return open(mmc_path, O_RDWR);
}

void mmc_init()
{
}

void mmc_finalize()
{
}

int mmc_is_path_on_device(const char *file_path, const char *device_path)
{
    // Not implemented
    return -1;
}

int mmc_trim(int fd, off_t offset, off_t count)
{
    // Not implemented
    fwup_warnx("TRIM command not implemented.");
    (void) fd;
    (void) offset;
    (void) count;
    return 0;
}

#endif // __FreeBSD__
