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

int mmc_umount_all(const char *mmc_device)
{
    WCHAR  VolumeName[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, mmc_device, 0, VolumeName, MAX_PATH);

    HANDLE volume_handle = CreateFile(VolumeName,
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL,
                                      OPEN_EXISTING,
                                      0,
                                      NULL);
    if (volume_handle == INVALID_HANDLE_VALUE) {
        fwup_warnx("Cannot open '%s'", mmc_device);
        return -1;
    }

    DWORD bytes_returned;
    BOOL status = DeviceIoControl(volume_handle,
                             FSCTL_LOCK_VOLUME,
                             NULL,
                             0,
                             NULL,
                             0,
                             &bytes_returned,
                             NULL);
    if (!status) {
        fwup_warnx("Error locking '%s'", mmc_device);
        CloseHandle(volume_handle);
        return -1;
    }

    status = DeviceIoControl(volume_handle,
                             FSCTL_DISMOUNT_VOLUME,
                             NULL,
                             0,
                             NULL,
                             0,
                             &bytes_returned,
                             NULL);
    if (!status) {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_SUPPORTED) {
            fwup_warnx("Error locking '%s'", mmc_device);
            CloseHandle(volume_handle);
            return -1;
        } else {
            fwup_warnx("Unmounting not supported");
        }
    }

    CloseHandle(volume_handle);

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
    WCHAR  VolumeName[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, mmc_path, 0, VolumeName, MAX_PATH);

    HANDLE volume_handle = CreateFile(VolumeName,
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL,
                                      OPEN_EXISTING,
                                      FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                                      NULL);
    if (volume_handle == INVALID_HANDLE_VALUE) {
        return -1;
    }

    DWORD bytes_returned;
    BOOL status = DeviceIoControl(volume_handle,
                             FSCTL_LOCK_VOLUME,
                             NULL,
                             0,
                             NULL,
                             0,
                             &bytes_returned,
                             NULL);
    if (!status) {
        CloseHandle(volume_handle);
        return -1;
    }

    return _open_osfhandle(volume_handle, 0);
}

#endif // defined(_WIN32) || defined(__CYGWIN__)
