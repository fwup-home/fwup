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
