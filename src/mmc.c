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

#include "mmc.h"
#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>

#define ONE_KiB  (1024ULL)
#define ONE_MiB  (1024 * ONE_KiB)
#define ONE_GiB  (1024 * ONE_MiB)

void mmc_pretty_size(size_t amount, char *out)
{
    if (amount >= ONE_GiB)
        sprintf(out, "%.2f GiB", ((double) amount) / ONE_GiB);
    else if (amount >= ONE_MiB)
        sprintf(out, "%.2f MiB", ((double) amount) / ONE_MiB);
    else if (amount >= ONE_KiB)
        sprintf(out, "%d KiB", (int) (amount / ONE_KiB));
    else
        sprintf(out, "%d bytes", (int) amount);
}

size_t mmc_device_size(const char *devpath)
{
    int fd = open(devpath, O_RDONLY);
    if (fd < 0)
        return 0;

    off_t len = lseek(fd, 0, SEEK_END);
    close(fd);

    return len < 0 ? 0 : len;
}

static bool is_mmc_device(const char *devpath)
{
    // Check 1: Path exists and can read length
    size_t len = mmc_device_size(devpath);
    if (len == 0)
        return false;

    // Check 2: Capacity larger than 32 GiB -> false
    if (len > (32 * ONE_GiB))
        return false;

    // Certainly there are more checks that we can do
    // to avoid false memory card detects...

    return true;
}

char *mmc_find_device()
{
    char *possible[64] = {0};
    size_t possible_ix = 0;
    char c;
    size_t i;

    // Scan memory cards connected via USB. These are /dev/sd_ devices.
    // NOTE: Don't scan /dev/sda, since I don't think this is ever right
    // for any use case.
    for (c = 'b'; c != 'z'; c++) {
        char devpath[64];
        sprintf(devpath, "/dev/sd%c", c);

        if (is_mmc_device(devpath) && possible_ix < NUM_ELEMENTS(possible))
            possible[possible_ix++] = strdup(devpath);
    }

    // Scan the mmcblk devices
    for (i = 0; i < 16; i++) {
        char devpath[64];
        sprintf(devpath, "/dev/mmcblk%d", (int) i);

        if (is_mmc_device(devpath) && possible_ix < NUM_ELEMENTS(possible))
            possible[possible_ix++] = strdup(devpath);
    }

    if (possible_ix == 1) {
        // Success.
        return possible[0];
    } else if (possible_ix == 0) {
        if (getuid() != 0)
            errx(EXIT_FAILURE, "Memory card couldn't be found automatically.\nTry running as root or specify -? for help");
        else
            errx(EXIT_FAILURE, "No memory cards found.");
    } else {
        fprintf(stderr, "Too many possible memory cards found: \n");
        for (i = 0; i < possible_ix; i++)
            fprintf(stderr, "  %s\n", possible[i]);
        fprintf(stderr, "Pick one and specify it explicitly on the commandline.\n");
        exit(EXIT_FAILURE);
    }
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

void mmc_umount_all(const char *mmc_device)
{
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp)
        err(EXIT_FAILURE, "/proc/mounts");

    char *todo[64] = {0};
    int todo_ix = 0;
    int i;

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
    for (i = 0; i < todo_ix; i++) {
        if (mtab_exists) {
            // If /etc/mtab, then call umount(8) so that
            // gets updated correctly.
            char cmdline[384];
            sprintf(cmdline, "/bin/umount %s", todo[i]);
            int rc = system(cmdline);
            if (rc != 0)
                err(EXIT_FAILURE, "%s", cmdline);
        } else {
            // No /etc/mtab, so call the kernel directly.
            if (umount(todo[i]) < 0)
                err(EXIT_FAILURE, "umount %s", todo[i]);
        }
    }

    for (i = 0; i < todo_ix; i++)
        free(todo[i]);
}

