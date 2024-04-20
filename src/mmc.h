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

#ifndef MMC_H
#define MMC_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

#include "util.h"

#define MMC_DEVICE_NAME_LEN 64
#define MMC_DEVICE_PATH_LEN 64
#define MMC_MAX_DEVICES 16

// NOTE: The rationale for this check is that the user's main drives will be
//       large capacity and we don't want to autodetect them when looking for
//       SDCards.
#define MMC_MAX_AUTODETECTED_SIZE (129 * ONE_GiB)
#define MMC_MIN_AUTODETECTED_SIZE (ONE_MiB)

struct mmc_device {
    char name[MMC_DEVICE_NAME_LEN];
    char path[MMC_DEVICE_PATH_LEN];
    off_t size;
};

/**
 * @brief Run any initialization required for other mmc_* functions.
 */
void mmc_init(void);

/**
 * @brief Free resources allocated by mmc_* functions.
 */
void mmc_finalize(void);

/**
 * @brief Scan for SDCards and other removable media
 * @param devices where to store detected devices and some metadata
 * @param max_devices the max to return
 * @return the number of devices found
 */
int mmc_scan_for_devices(struct mmc_device *devices, int max_devices);

/**
 * @brief Return the size of an SDCard/MMC device
 * @param mmc_path the path
 * @param end_offset the size of the device
 * @return <0 on error
 */
int mmc_device_size(const char *mmc_path, off_t *end_offset);

/**
 * @brief Open an SDCard/MMC device
 * @param mmc_path the path
 * @return a filehandle or <0 on error
 */
int mmc_open(const char *mmc_path);

/**
 * @brief Unmount everything that's mounted on mmc_path
 * @param mmc_path a device path
 * @return <0 on error
 */
int mmc_umount_all(const char *mmc_path);

/**
 * @brief Eject the specified device (if supported)
 * @param mmc_device the device path
 * @return <0 on error
 */
int mmc_eject(const char *mmc_device);

/**
 * @brief Check if the specified file_path is on a device
 *
 * This function doesn't try very hard. It only runs "stat(2)" or
 * the equivalent to see whether the path's containing device
 * is the same as device_path.
 *
 * @param file_path any path (e.g., "/")
 * @param device_path the device path (e.g., "/dev/mmcblk0p1")
 * @return 1 if yes, 0 if no, <0 on error
 */
int mmc_is_path_on_device(const char *file_path, const char *device_path);

/**
 * @brief Check if the specified file_path is mounted on the specified device and offfset
 *
 * This function doesn't try very hard. It only runs "stat(2)" or
 * the equivalent to see whether the path's containing device
 * is the same as device_path.
 *
 * @param file_path any path (e.g., "/")
 * @param device_path the device path (e.g., "/dev/mmcblk0")
 * @param block_offset the offset on the device
 * @return 1 if yes, 0 if no, <0 on error
 */
int mmc_is_path_at_device_offset(const char *file_path, off_t block_offset);

/**
 * @brief Issue a trim command to the output filesystem
 * @param fd
 * @param offset
 * @param count
 * @return
 */
int mmc_trim(int fd, off_t offset, off_t count);

#endif // MMC_H
