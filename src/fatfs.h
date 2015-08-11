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

#ifndef FATFS_H
#define FATFS_H

#include <stdio.h>

/*
 * API wrappers to make the FatFS library easier to use.
 */

struct tm;
int fatfs_set_time(struct tm *tmp);

int fatfs_mkfs(FILE *fatfp, off_t fatfp_offset, int block_count);
int fatfs_attrib(FILE *fatfp, off_t fatfp_offset, const char *filename, const char *attrib);
int fatfs_mkdir(FILE *fatfp, off_t fatfp_offset, const char *dir);
int fatfs_setlabel(FILE *fatfp, off_t fatfp_offset, const char *label);
int fatfs_mv(FILE *fatfp, off_t fatfp_offset, const char *from_name, const char *to_name);
int fatfs_rm(FILE *fatfp, off_t fatfp_offset, const char *filename);
int fatfs_pwrite(FILE *fatfp, off_t fatfp_offset, const char *filename, int offset, const char *buffer, off_t size);
int fatfs_cp(FILE *fatfp, off_t fatfp_offset, const char *from_name, const char *to_name);
int fatfs_closefs();

#endif // FATFS_H
