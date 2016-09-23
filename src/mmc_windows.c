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

#if defined(_WIN32) || defined(__CYGWIN__)

#define UNICODE
#include <windows.h>
#include <io.h>
#include <stdlib.h>
#include <wchar.h>

#include "mmc.h"
#include "util.h"

void mmc_init()
{
}

void mmc_finalize()
{
}

/**
 * @brief Scan for SDCards and other removable media
 * @param devices where to store detected devices and some metadata
 * @param max_devices the max to return
 * @return the number of devices found
 */
int mmc_scan_for_devices(struct mmc_device *devices, int max_devices)
{
    // There's not an API to request all the PhysicalDrives, but they are
    // sequentially enumerated and become invalid if they're removed.
    // So we just scan the first 256 of them until we find enough matches
    int device_count = 0;
    for (int i = 0; i < 256 && device_count < max_devices ; i++) {
        WCHAR drive_path[MAX_PATH] = L"";
        wsprintf(drive_path, L"\\\\.\\PhysicalDrive%d", i);

        HANDLE drive_handle;
        drive_handle = CreateFile(drive_path,
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL,
                                  OPEN_EXISTING,
                                  0,
                                  NULL);

        if (drive_handle == INVALID_HANDLE_VALUE) {
            CloseHandle(drive_handle);
            continue;
        }

        DWORD bytes_returned;
        DISK_GEOMETRY_EX geometry;
        BOOL status = DeviceIoControl(drive_handle,
                                      IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                                      NULL,
                                      0,
                                      &geometry,
                                      sizeof(geometry),
                                      &bytes_returned,
                                      NULL);
        CloseHandle(drive_handle);

        if (status && geometry.Geometry.MediaType == RemovableMedia && geometry.DiskSize.QuadPart > 0) {
            struct mmc_device *device;
            device = &devices[device_count];
            WideCharToMultiByte(CP_UTF8, 0, drive_path, -1, device->path, sizeof(device->path), NULL, NULL);
            device->size = geometry.DiskSize.QuadPart;
            device_count++;
        }
    }

    return device_count;
}

/*
 * @brief Query the Physical Drive(s) that back a Logical Volume
 * Since we're trying dealing with SD cards, ignore any
 * volumes that span multiple Physical Drives.
 * @param volume_name the name of the volume as a Wide String
 * @return the PhysicalDrive Number on success, 0 on failure
 */
static unsigned int query_physical_extents(LPWSTR volume_name) {
    // We have to remove the trailing slash for this API call
    volume_name[wcslen(volume_name) - 1] = L'\0';
    HANDLE volume_handle = CreateFile(volume_name,
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL,
                                      OPEN_EXISTING,
                                      0,
                                      NULL);
    if (volume_handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    VOLUME_DISK_EXTENTS extents;
    DWORD bytes_returned;
    BOOL status = DeviceIoControl(volume_handle,
                                  IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                                  NULL,
                                  0,
                                  &extents,
                                  sizeof(extents),
                                  &bytes_returned,
                                  NULL);
    CloseHandle(volume_handle);

    if (!status || extents.NumberOfDiskExtents > 1) {
        // Ignore it if we can't query its extents or it is backed by
        // more than one extent because it's probably not an SD card.
        return 0;
    }

    return extents.Extents[0].DiskNumber;
}

/*
 * @brief Attempt to unmount the specified volume, exit with failure message if unsuccessful
 * @param volume_name the name of the volume as a Wide String
 */
static void unmount_volume(LPWSTR volume_name) {
    HANDLE volume_handle = CreateFile(volume_name,
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL,
                                      OPEN_EXISTING,
                                      0,
                                      NULL);
    if (volume_handle == INVALID_HANDLE_VALUE)
        fwup_errx(EXIT_FAILURE, "Could not open '%S' for unmounting (Error %lu)", volume_name, GetLastError());

    DWORD bytes_returned;
    BOOL status = DeviceIoControl(volume_handle,
                                  FSCTL_LOCK_VOLUME,
                                  NULL,
                                  0,
                                  NULL,
                                  0,
                                  &bytes_returned,
                                  NULL);

    if (!status)
        fwup_warnx("Could not lock '%S' for unmounting (Error %lu)", volume_name, GetLastError());

    status = DeviceIoControl(volume_handle,
                             FSCTL_DISMOUNT_VOLUME,
                             NULL,
                             0,
                             NULL,
                             0,
                             &bytes_returned,
                             NULL);


    if (!status)
        fwup_errx(EXIT_FAILURE, "Error unmounting '%S' (Error %lu)", volume_name, GetLastError());

    // Note we deliberately do not FSCTL_UNLOCK_VOLUME or call CloseHandle, as the Logical Volume must
    // remained locked.  We rely on Windows to cleanup for us
}

/*
 * @brief Unmount all LogicalVolumes using the specified PhysicalDrive
 * @param mmc_device the name of the PhysicalDrive
 * @return 0 on success, exit the program with a failure message otherwise
 */
int mmc_umount_all(const char *mmc_device)
{
    unsigned int target_disk_number = 0;
    sscanf(mmc_device, "\\\\.\\PhysicalDrive%u", &target_disk_number);
    if (target_disk_number == 0)
        fwup_errx(EXIT_FAILURE, "Target device must be formatted like \\\\.\\PhysicalDisk# where # is a positive integer.");

    WCHAR volume_name[MAX_PATH] = L"";
    HANDLE volume_iter = FindFirstVolume(volume_name, ARRAYSIZE(volume_name));

    if (volume_iter == INVALID_HANDLE_VALUE)
        fwup_errx(EXIT_FAILURE, "Can't enumerate logical volumes (Error %lu)", GetLastError());

    do {
        unsigned int disk_number = query_physical_extents(volume_name);
        if (disk_number == target_disk_number)
            unmount_volume(volume_name);
    } while (FindNextVolume(volume_iter, volume_name, ARRAYSIZE(volume_name)));

    FindVolumeClose(volume_iter);

    return 0;
}

int mmc_eject(const char *mmc_device)
{
    // Windows doesn't complain if you don't eject an unmounted device
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
    WCHAR drive_name[MAX_PATH] = L"";
    MultiByteToWideChar(CP_UTF8, 0, mmc_path, -1, drive_name, MAX_PATH);

    HANDLE drive_handle = CreateFile(drive_name,
                                     GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL,
                                     OPEN_EXISTING,
                                     FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                                     NULL);
    if (drive_handle == INVALID_HANDLE_VALUE) {
        fwup_warnx("Unable to open drive %S\n (%lu)", drive_name, GetLastError());
        return -1;
    }

    return _open_osfhandle((intptr_t) drive_handle, 0);
}

#endif // defined(_WIN32) || defined(__CYGWIN__)
