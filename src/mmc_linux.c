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

static bool is_mmc_device(off_t device_size)
{
    // Check 1: Path exists and can read length
    if (device_size == 0)
        return false;

    // Check 2: Capacity larger than 65 GiB -> false
    if (device_size > (65 * ONE_GiB))
        return false;

    return true;
}

/**
 * @brief Scan for SDCards and other removable media
 * @param devices where to store detected devices and some metadata
 * @param max_devices the max to return
 * @return the number of devices found
 */
int mmc_scan_for_devices(struct mmc_device *devices, int max_devices)
{
    int device_count = 0;

    // Scan memory cards connected via USB. These are /dev/sd_ devices.
    // NOTE: Don't scan /dev/sda, since I don't think this is ever right
    // for any use case.
    for (char c = 'b'; c != 'z'; c++) {
        char devpath[16];
        sprintf(devpath, "/dev/sd%c", c);

        off_t device_size = mmc_device_size_raw(devpath);
        if (device_size == 0) {
            char sysfspath[32];
            sprintf(sysfspath, "/sys/block/sd%c/size", c);
            device_size = mmc_device_size_sysfs(sysfspath);
        }
        if (is_mmc_device(device_size) && device_count < max_devices) {
            strcpy(devices[device_count].path, devpath);
            devices[device_count].size = device_size;
            device_count++;
        }
    }

    // Scan the mmcblk devices
    for (int i = 0; i < 16; i++) {
        char devpath[16];
        sprintf(devpath, "/dev/mmcblk%d", i);

        off_t device_size = mmc_device_size_raw(devpath);
        if (device_size == 0) {
            char sysfspath[32];
            sprintf(sysfspath, "/sys/block/mmcblk%d/size", i);
            device_size = mmc_device_size_sysfs(sysfspath);
        }
        if (is_mmc_device(device_size) && device_count < max_devices) {
            strcpy(devices[device_count].path, devpath);
            devices[device_count].size = device_size;
            device_count++;
        }
    }

    return device_count;
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

int mmc_umount_all(const char *mmc_device)
{
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp)
        err(EXIT_FAILURE, "/proc/mounts");

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
                errx(EXIT_FAILURE, "Device mounted too many times");

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
            char cmdline[384];
            sprintf(cmdline, "/bin/umount %s", todo[i]);
            int rc = system(cmdline);
            if (rc != 0) {
                warnx("%s", cmdline);
                ultimate_rc = -1;
            }
        } else {
            // No /etc/mtab, so call the kernel directly.
#if HAS_UMOUNT
            if (umount(todo[i]) < 0) {
                warnx("umount %s", todo[i]);
                ultimate_rc = -1;
            }
#else
            // If no umount on this platform, warn, but don't
            // return failure.
            warnx("umount %s: not supported", todo[i]);
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

#endif // __linux__
