#include "fatfs.h"

#include "fatfs/src/diskio.h"		/* FatFs lower layer API */
#include "fatfs/src/ff.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

// Globals since that's how the FatFS code like to work.
static FILE *fatfp_ = NULL;
static int block_count_ = 0;
static char *current_file_ = NULL;
static FATFS fs_;
static FIL fil_;
static FRESULT last_error_ = FR_OK;

#define CHECK(CMD) do { if ((last_error_ = CMD) != FR_OK) return -1; } while (0)
#define MAYBE_MOUNT(FATFP) do { if (fatfp_ != FATFP) { fatfp_ = FATFP; CHECK(f_mount(&fs_, "", 0)); } } while (0)

/**
 * @brief fatfs_mkfs Make a new FAT filesystem
 * @param fatfp the file to contain the raw filesystem data
 * @param block_count how many 512 blocks
 * @return 0 on success
 */
int fatfs_mkfs(FILE *fatfp, int block_count)
{
    // The block count is only used for f_mkfs according to the docs.
    block_count_ = block_count;

    MAYBE_MOUNT(fatfp);
    CHECK(f_mkfs("", 1, 0));

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
 * @param fatfp the raw file system data
 * @param dir the name of the directory
 * @return 0 on success
 */
int fatfs_mkdir(FILE *fatfp, const char *dir)
{
    MAYBE_MOUNT(fatfp);
    close_open_files();
    CHECK(f_mkdir(dir));
    return 0;
}

/**
 * @brief fatfs_mv rename a file
 * @param fatfp the raw file system data
 * @param from_name original filename
 * @param to_name new filename
 * @return 0 on success
 */
int fatfs_mv(FILE *fatfp, const char *from_name, const char *to_name)
{
    MAYBE_MOUNT(fatfp);
    close_open_files();
    CHECK(f_rename(from_name, to_name));
    return 0;
}

int fatfs_pwrite(FILE *fatfp, const char *filename, int offset, const char *buffer, size_t size)
{
    MAYBE_MOUNT(fatfp);

    // Check if this is the same file as a previous pwrite call
    if (current_file_ && strcmp(current_file_, filename) != 0)
        close_open_files();

    if (!current_file_) {
        CHECK(f_open(&fil_, filename, FA_CREATE_NEW | FA_WRITE));

        // Assuming it opens ok, cache the filename for future writes.
        current_file_ = strdup(filename);
    }

    CHECK(f_lseek(&fil_, offset));

    UINT bw;
    CHECK(f_write(&fil_, buffer, size, &bw));
    if (size != bw)
        return FR_DENIED;
    return 0;
}

int fatfs_closefs()
{
    close_open_files();

    // This unmounts. Don't check error.
    f_mount(NULL, "", 0);
    fatfp_ = NULL;
    return 0;
}

const char *fatfs_last_error()
{
    switch (last_error_) {
    case FR_OK: return "Succeeded";
    case FR_DISK_ERR: return "A hard error occurred in the low level disk I/O layer";
    case FR_INT_ERR: return "Assertion failed";
    case FR_NOT_READY: return "The physical drive cannot work";
    case FR_NO_FILE: return "Could not find the file";
    case FR_NO_PATH: return "Could not find the path";
    case FR_INVALID_NAME: return "The path name format is invalid";
    case FR_DENIED: return "Access denied due to prohibited access or directory full";
    case FR_EXIST: return "Access denied due to prohibited access";
    case FR_INVALID_OBJECT: return "The file/directory object is invalid";
    case FR_WRITE_PROTECTED: return "The physical drive is write protected";
    case FR_INVALID_DRIVE: return "The logical drive number is invalid";
    case FR_NOT_ENABLED: return "The volume has no work area";
    case FR_NO_FILESYSTEM: return "There is no valid FAT volume";
    case FR_MKFS_ABORTED: return "The f_mkfs() aborted due to any parameter error";
    case FR_TIMEOUT: return "Could not get a grant to access the volume within defined period";
    case FR_LOCKED: return "The operation is rejected according to the file sharing policy";
    case FR_NOT_ENOUGH_CORE: return "LFN working buffer could not be allocated";
    case FR_TOO_MANY_OPEN_FILES: return "Number of open files > _FS_SHARE";
    default:
    case FR_INVALID_PARAMETER: return "Invalid";
    }
}

// Implementation of callbacks
DSTATUS disk_initialize(BYTE pdrv)				/* Physical drive nmuber (0..) */
{
    return pdrv == 0 ? 0 : STA_NODISK;
}

DSTATUS disk_status(BYTE pdrv)		/* Physical drive nmuber (0..) */
{
    return pdrv == 0 && fatfp_ ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv,		/* Physical drive nmuber (0..) */
                  BYTE *buff,		/* Data buffer to store read data */
                  DWORD sector,	/* Sector address (LBA) */
                  UINT count)		/* Number of sectors to read (1..128) */
{
    if (pdrv != 0 || !fatfp_)
        return RES_PARERR;

    size_t byte_offset = sector * 512;
    size_t byte_count = count * 512;

    if (fseek(fatfp_, byte_offset, SEEK_SET) < 0)
        return RES_ERROR;

    ssize_t amount_read = fread(buff, 1, byte_count, fatfp_);
    if (amount_read < 0)
        amount_read = 0;

    if ((size_t) amount_read != byte_count)
        memset(&buff[amount_read], 0, byte_count - amount_read);

    return 0;
}

DRESULT disk_write(BYTE pdrv,			/* Physical drive nmuber (0..) */
                   const BYTE *buff,	/* Data to be written */
                   DWORD sector,		/* Sector address (LBA) */
                   UINT count)			/* Number of sectors to write (1..128) */
{
    if (pdrv != 0 || !fatfp_)
        return RES_PARERR;

    size_t byte_offset = sector * 512;
    size_t byte_count = count * 512;

    if (fseek(fatfp_, byte_offset, SEEK_SET) < 0)
        return RES_ERROR;

    size_t amount_written = fwrite(buff, 1, byte_count, fatfp_);
    if (amount_written != byte_count)
        return RES_ERROR;

    return 0;
}

DRESULT disk_ioctl(BYTE pdrv,		/* Physical drive nmuber (0..) */
                   BYTE cmd,		/* Control code */
                   void *buff)		/* Buffer to send/receive control data */
{
    if (pdrv != 0 || !fatfp_)
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
    case CTRL_ERASE_SECTOR:
    default:
        return RES_PARERR;
    }

    return RES_PARERR;
}

DWORD get_fattime()
{
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    if (tmp == NULL) {
        perror("localtime");
        exit(1);
    }

    // Return the time the FAT way.
    DWORD fattime = ((tmp->tm_year - 80) << 25) |
            (tmp->tm_mon << 21) |
            (tmp->tm_mday << 16) |
            (tmp->tm_hour << 11) |
            (tmp->tm_min << 5) |
            (tmp->tm_sec >> 1);

    return fattime;
}
