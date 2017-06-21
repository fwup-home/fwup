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

#define _GNU_SOURCE // for memmem in string.h

#include "fatfs.h"

#include "../3rdparty/fatfs/src/diskio.h"  /* FatFs lower layer API */
#include "../3rdparty/fatfs/src/ff.h"
#include "util.h"
#include "block_cache.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Globals since that's how the FatFS code likes to work.
static struct block_cache *output_;
static off_t block_offset_;
static int block_count_ = 0;
static char *current_file_ = NULL;
static FATFS fs_;
static FIL fil_;
static DWORD fattime_;

const char *fatfs_error_to_string(FRESULT err)
{
    switch (err) {
    case FR_OK: return "Succeeded";
    case FR_DISK_ERR: return "A hard error occurred in the low level disk I/O layer";
    case FR_INT_ERR: return "Assertion failed";
    case FR_NOT_READY: return "The physical drive cannot work";
    case FR_NO_FILE: return "Could not find the file";
    case FR_NO_PATH: return "Could not find the path";
    case FR_INVALID_NAME: return "The path name format is invalid";
    case FR_DENIED: return "Access denied due to prohibited access or directory full";
    case FR_EXIST: return "Destination file already exists";
    case FR_INVALID_OBJECT: return "The file/directory object is invalid";
    case FR_WRITE_PROTECTED: return "The physical drive is write protected";
    case FR_INVALID_DRIVE: return "The logical drive number is invalid";
    case FR_NOT_ENABLED: return "The volume has no work area";
    case FR_NO_FILESYSTEM: return "There is no valid FAT volume";
    case FR_MKFS_ABORTED: return "f_mkfs() aborted due to a parameter error. Try adjusting the partition size.";
    case FR_TIMEOUT: return "Could not get a grant to access the volume within defined period";
    case FR_LOCKED: return "The operation is rejected according to the file sharing policy";
    case FR_NOT_ENOUGH_CORE: return "LFN working buffer could not be allocated";
    case FR_TOO_MANY_OPEN_FILES: return "Number of open files > _FS_SHARE";
    default:
    case FR_INVALID_PARAMETER: return "Invalid";
    }
}

static FRESULT fatfs_error(const char *context, const char *filename, FRESULT rc)
{
    if (rc != FR_OK) {
        set_last_error("%s(%s): %s", context, filename ? filename : "", fatfs_error_to_string(rc));
    }

    return rc;
}

#define CHECK(CONTEXT, FILENAME, CMD) do { if (fatfs_error(CONTEXT, FILENAME, CMD) != FR_OK) return -1; } while (0)
#define CHECK_CLEANUP(CONTEXT, FILENAME, CMD) do { if (fatfs_error(CONTEXT, FILENAME, CMD) != FR_OK) { rc = -1; goto cleanup; } } while (0)
#define MAYBE_MOUNT(BLOCK_CACHE, BLOCK_OFFSET) do { if (output_ != BLOCK_CACHE || block_offset_ != BLOCK_OFFSET) { output_ = BLOCK_CACHE; block_offset_ = BLOCK_OFFSET; CHECK("fat_mount", NULL, f_mount(&fs_, "", 0)); } } while (0)

/**
 * @brief fatfs_mkfs Make a new FAT filesystem
 * @param block_writer the file to contain the raw filesystem data
 * @param block_offset the offset within fatfp for where to start
 * @param block_count how many FWUP_BLOCK_SIZE blocks
 * @return 0 on success
 */
int fatfs_mkfs(struct block_cache *output, off_t block_offset, size_t block_count)
{
    // The block count is only used for f_mkfs according to the docs. Store
    // it here for the call to f_mkfs since there's no way to pass it through.
    block_count_ = block_count;

    MAYBE_MOUNT(output, block_offset);

    // Since we're going to format, clear out all blocks in the cache
    // in the formatted range. Additionally, mark these blocks so that
    // they don't need to be written to disk. If the format code writes
    // to them, they'll be marked dirty. However, if any code tries to
    // read them, they'll get back zeros without any I/O. This is best
    // effort.
    OK_OR_RETURN_MSG(block_cache_trim(output, block_offset * FWUP_BLOCK_SIZE, block_count * FWUP_BLOCK_SIZE, true),
                     "Error trimming blocks affacted by fat_mkfs");

    // The third parameter is the cluster size. We set it low so
    // that we have enough clusters to easily bump the cluster count
    // above the FAT32 threshold. The minimum number of clusters to
    // get FAT32 is 65526. This is important for the Raspberry Pi since
    // it only boots off FAT32 partitions and we don't want a huge
    // boot partition.
    //
    // NOTE2: FAT file system usage with fwup generally has been for the small
    // boot partitions on platforms and not huge partitions. If this
    // changes, it would be good to make this configurable so that massive
    // partitions could be made.
    //
    // NOTE3: Specify FM_SFD (super-floppy disk) to avoid fatfs wanting to create
    // a master boot record.
    char buffer[_MAX_SS];
    CHECK("fat_mkfs", NULL, f_mkfs("", FM_SFD|FM_FAT|FM_FAT32, FWUP_BLOCK_SIZE, buffer, sizeof(buffer)));

    return 0;
}

// Helper method to close the file that we keep open to avoid unnecessary
// FAT chain traversals.
static void close_open_files()
{
    if (current_file_) {
        f_close(&fil_);
        free(current_file_);
        current_file_ = NULL;
    }
}

/**
 * @brief fatfs_mkdir Make a directory
 * @param fc the current FAT session
 * @param dir the name of the directory
 * @return 0 on success
 */
int fatfs_mkdir(struct block_cache *output, off_t block_offset, const char *dir)
{
    MAYBE_MOUNT(output, block_offset);
    close_open_files();

    // Check if the directory already exists and is a directory.
    FILINFO info;
    FRESULT rc = f_stat(dir, &info);
    if (rc == FR_OK && info.fattrib & AM_DIR)
        return 0;

    // Try to make it if not.
    CHECK("fat_mkdir", dir, f_mkdir(dir));
    return 0;
}

/**
 * @brief fatfs_setlabel Set the volume label
 * @param fc the current FAT session
 * @param label the name of the filesystem
 * @return 0 on success
 */
int fatfs_setlabel(struct block_cache *output, off_t block_offset, const char *label)
{
    MAYBE_MOUNT(output, block_offset);
    close_open_files();
    CHECK("fat_setlabel", label, f_setlabel(label));
    return 0;
}

/**
 * @brief fatfs_rm Delete a file
 * @param fc the current FAT session
 * @param cmd the command name for error messages
 * @param filename the name of the file
 * @param file_must_exist true if the file must exist
 * @return 0 on success
 */
int fatfs_rm(struct block_cache *output, off_t block_offset, const char *cmd, const char *filename, bool file_must_exist)
{
    MAYBE_MOUNT(output, block_offset);
    close_open_files();

    FRESULT rc = f_unlink(filename);
    switch (rc) {
    case FR_OK:
        return 0;

    case FR_NO_FILE:
        if (!file_must_exist)
            return 0;

        // fall through
    default:
        fatfs_error(cmd, filename, rc);
        return -1;
    }
}

/**
 * @brief fatfs_mv rename a file
 * @param fc the current FAT session
 * @param cmd the command name for error messages
 * @param from_name original filename
 * @param to_name new filename
 * @param force set to true to rename the file even if to_name exists
 * @return 0 on success
 */
int fatfs_mv(struct block_cache *output, off_t block_offset, const char *cmd, const char *from_name, const char *to_name, bool force)
{
    MAYBE_MOUNT(output, block_offset);
    close_open_files();

    // If forcing, remove the file first.
    if (force && fatfs_rm(output, block_offset, cmd, to_name, false))
        return -1;

    CHECK(cmd, from_name, f_rename(from_name, to_name));

    return 0;
}

/**
 * @brief fatfs_cp copy a file
 * @param fc the current FAT session
 * @param from_name original filename
 * @param to_name the name of the copy filename
 * @return 0 on success
 */
int fatfs_cp(struct block_cache *output, off_t block_offset, const char *from_name, const char *to_name)
{
    MAYBE_MOUNT(output, block_offset);
    close_open_files();

    FIL fromfil;
    FIL tofil;
    CHECK("fatfs_cp can't open file", from_name, f_open(&fromfil, from_name, FA_READ));
    CHECK("fatfs_cp can't open file", to_name, f_open(&tofil, to_name, FA_CREATE_NEW | FA_WRITE));

    for (;;) {
        char buffer[4096];
        UINT bw, br;

        CHECK("fatfs_cp can't read", from_name, f_read(&fromfil, buffer, sizeof(buffer), &br));
        if (br == 0)
            break;

        CHECK("fatfs_cp can't write", to_name, f_write(&tofil, buffer, br, &bw));
        if (br != bw)
            ERR_RETURN("Error copying file to FAT");

    }
    f_close(&fromfil);
    f_close(&tofil);
    return 0;
}

/**
 * @brief fatfs_attrib set the attribs on a file
 * @param fc the current FAT session
 * @param filename an existing file
 * @param attrib a string with the attributes. i.e., "RHS"
 * @return 0 on success
 */
int fatfs_attrib(struct block_cache *output, off_t block_offset, const char *filename, const char *attrib)
{
    MAYBE_MOUNT(output, block_offset);

    BYTE mode = 0;
    while (*attrib) {
        switch (*attrib++) {
        case 'S':
        case 's':
            mode |= AM_SYS;
            break;
        case 'H':
        case 'h':
            mode |= AM_HID;
            break;
        case 'R':
        case 'r':
            mode |= AM_RDO;
            break;
        }
    }
    CHECK("fat_attrib", filename, f_chmod(filename, mode, AM_RDO | AM_HID | AM_SYS));
    return 0;
}

/**
 * @brief fatfs_touch create an empty file if the file doesn't exist
 * @param fc the current FAT session
 * @param filename the file to touch
 * @return 0 on success
 */
int fatfs_touch(struct block_cache *output, off_t block_offset, const char *filename)
{
    MAYBE_MOUNT(output, block_offset);
    close_open_files();

    FIL fil;
    CHECK("fat_touch", filename, f_open(&fil, filename, FA_OPEN_ALWAYS));
    f_close(&fil);

    return 0;
}

/**
 * @brief fatfs_exists check if the specified file exists
 * @param fc the current FAT session
 * @param filename the filename
 * @return 0 if it exists
 */
int fatfs_exists(struct block_cache *output, off_t block_offset, const char *filename)
{
    MAYBE_MOUNT(output, block_offset);
    close_open_files();

    FIL fil;
    CHECK("fatfs_exists", filename, f_open(&fil, filename, FA_OPEN_EXISTING));
    f_close(&fil);

    return 0;
}

/**
 * @brief fatfs_file_matches check if the pattern can be found in filename
 * @param fc the current FAT session
 * @param filename the file to search through
 * @param pattern the pattern to patch
 * @return 0 if the file exists and the pattern is inside of it
 */
int fatfs_file_matches(struct block_cache *output, off_t block_offset, const char *filename, const char *pattern)
{
    MAYBE_MOUNT(output, block_offset);
    close_open_files();

    FIL fil;
    CHECK("fatfs_file_matches can't open file", filename, f_open(&fil, filename, FA_READ));

    char buffer[4096];
    int rc = -1;
    size_t pattern_len = strlen(pattern);
    if (pattern_len >= sizeof(buffer))
        goto cleanup;
    if (pattern_len == 0) {
        // 0-length patterns always match if the file exists.
        rc = 0;
        goto cleanup;
    }
    size_t offset = 0;
    for (;;) {
        UINT br;

        CHECK_CLEANUP("fatfs_file_matches", filename, f_read(&fil, buffer + offset, sizeof(buffer) - offset, &br));
        if (br == 0)
            break;

        if (memmem(buffer, br, pattern, pattern_len) != 0) {
            // Found it.
            rc = 0;
            break;
        }

        // Handle pattern matches between this buffer and the next one.
        if (br >= pattern_len) {
            // Copy the last (pattern_len - 1) bytes to the beginning and read
            // from there next time.
            offset = pattern_len - 1;
            memcpy(buffer, buffer + br - offset, offset);
        } else {
            // Read less than pattern_len, so read more from there next time
            // assuming that there is a next time.
            offset = br;
        }
    }
cleanup:
    f_close(&fil);

    return rc;
}

int fatfs_pwrite(struct block_cache *output, off_t block_offset,const char *filename, int offset, const char *buffer, off_t size)
{
    MAYBE_MOUNT(output, block_offset);

    // Check if this is the same file as a previous pwrite call
    if (current_file_ && strcmp(current_file_, filename) != 0)
        close_open_files();

    if (!current_file_) {
        CHECK("fat_write can't open file", filename, f_open(&fil_, filename, FA_CREATE_NEW | FA_WRITE));

        // Assuming it opens ok, cache the filename for future writes.
        current_file_ = strdup(filename);
    }

    // Check if this pwrite requires a seek.
    DWORD desired_offset = offset;
    if (desired_offset != f_tell(&fil_)) {
        // Need to seek, but if we're seeking past the end, be sure to fill in with zeros.
        if (desired_offset > f_size(&fil_)) {
            // Seek to the end
            CHECK("fat_write can't seek to end of file", filename, f_lseek(&fil_, f_size(&fil_)));

            // Write zeros.
            DWORD zero_count = desired_offset - f_tell(&fil_);
            char zero_buffer[FWUP_BLOCK_SIZE];
            memset(zero_buffer, 0, sizeof(zero_buffer));
            while (zero_count) {
                DWORD btw = (zero_count < sizeof(zero_buffer) ? zero_count : sizeof(zero_buffer));
                UINT bw;
                CHECK("fat_write can't write", filename, f_write(&fil_, zero_buffer, btw, &bw));
                if (btw != bw)
                    ERR_RETURN("Error writing file to FAT: %s, expected %ld bytes written, got %d (maybe the disk is full?)", filename, size, bw);
                zero_count -= bw;
            }
        } else {
            CHECK("fat_write can't seek in file", filename, f_lseek(&fil_, desired_offset));
        }
    }

    UINT bw;
    CHECK("fat_write can't write", filename, f_write(&fil_, buffer, size, &bw));

    if (size != bw)
        ERR_RETURN("Error writing file to FAT: %s, expected %ld bytes written, got %d (maybe the disk is full?)", filename, size, bw);

    return 0;
}

void fatfs_closefs()
{
    if (output_) {
        close_open_files();

        // This unmounts. Don't check error.
        f_mount(NULL, "", 0);
        output_ = NULL;
    }
}

// Implementation of callbacks
DSTATUS disk_initialize(BYTE pdrv)				/* Physical drive number (0..) */
{
    return pdrv == 0 ? 0 : STA_NODISK;
}

DSTATUS disk_status(BYTE pdrv)		/* Physical drive number (0..) */
{
    return (pdrv == 0 && output_) ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv,		/* Physical drive number (0..) */
                  BYTE *buff,		/* Data buffer to store read data */
                  DWORD sector,	/* Sector address (LBA) */
                  UINT count)		/* Number of sectors to read (1..128) */
{
    if (pdrv != 0 || output_ == NULL)
        return RES_PARERR;

    if (block_cache_pread(output_, buff, FWUP_BLOCK_SIZE * count, FWUP_BLOCK_SIZE * (block_offset_ + sector)) < 0)
        return RES_ERROR;
    else
        return 0;
}

DRESULT disk_write(BYTE pdrv,			/* Physical drive number (0..) */
                   const BYTE *buff,	/* Data to be written */
                   DWORD sector,		/* Sector address (LBA) */
                   UINT count)			/* Number of sectors to write (1..128) */
{
    if (pdrv != 0 || output_ == NULL)
        return RES_PARERR;

    if (block_cache_pwrite(output_, buff, FWUP_BLOCK_SIZE * count, FWUP_BLOCK_SIZE * (block_offset_ + sector), false) < 0)
        return RES_ERROR;
    else
        return 0;
}

DRESULT disk_ioctl(BYTE pdrv,		/* Physical drive number (0..) */
                   BYTE cmd,		/* Control code */
                   void *buff)		/* Buffer to send/receive control data */
{
    if (pdrv != 0 || output_ == NULL)
        return RES_PARERR;

    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;

    case GET_SECTOR_COUNT:
    {
        DWORD *n_vol = (DWORD *) buff;
        *n_vol = block_count_;
        return RES_OK;
    }
    case GET_BLOCK_SIZE:
    {
        // This is called, but I don't believe that it changes anything for us.
        DWORD *n = (DWORD *) buff;
        *n = 128 * 1024;
        return RES_OK;
    }
    case GET_SECTOR_SIZE:
    case CTRL_TRIM:
    default:
        return RES_PARERR;
    }

    return RES_PARERR;
}

int fatfs_set_time(struct tm *tmp)
{
    // See the fatfs documentation for the format or believe me.
    fattime_ = ((tmp->tm_year - 80) << 25) |
            ((tmp->tm_mon + 1) << 21) |
            (tmp->tm_mday << 16) |
            (tmp->tm_hour << 11) |
            (tmp->tm_min << 5) |
            (tmp->tm_sec >> 1);

    return 0;
}

DWORD get_fattime()
{
    return fattime_;
}
