#ifndef FATFS_H
#define FATFS_H

#include <stdio.h>

/*
 * API wrappers to make the FatFS library easier to use.
 */

int fatfs_mkfs(FILE *fatfp, int block_count);
int fatfs_mkdir(FILE *fatfp, const char *dir);
int fatfs_mv(FILE *fatfp, const char *from_name, const char *to_name);
int fatfs_pwrite(FILE *fatfp, const char *filename, int offset, const char *buffer, size_t size);
int fatfs_closefs();
const char *fatfs_last_error();

#endif // FATFS_H
