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

#ifndef MMC_H
#define MMC_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#define MMC_DEVICE_PATH_LEN 32

struct mmc_device {
    char path[MMC_DEVICE_PATH_LEN];
    off_t size;
};

void mmc_init();
void mmc_finalize();

int mmc_scan_for_devices(struct mmc_device *devices, int max_devices);

int mmc_open(const char *mmc_path);

int mmc_umount_all(const char *mmc_path);
int mmc_eject(const char *mmc_device);

#endif // MMC_H
