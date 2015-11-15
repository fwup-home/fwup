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

#include "fat_cache.h"

/*
 * API wrappers to make the FatFS library easier to use.
 */

struct tm;
int fatfs_set_time(struct tm *tmp);

int fatfs_mkfs(struct fat_cache *fc, int block_count);
int fatfs_attrib(struct fat_cache *fc, const char *filename, const char *attrib);
int fatfs_mkdir(struct fat_cache *fc, const char *dir);
int fatfs_setlabel(struct fat_cache *fc, const char *label);
int fatfs_mv(struct fat_cache *fc, const char *from_name, const char *to_name);
int fatfs_rm(struct fat_cache *fc, const char *filename);
int fatfs_pwrite(struct fat_cache *fc, const char *filename, int offset, const char *buffer, off_t size);
int fatfs_cp(struct fat_cache *fc, const char *from_name, const char *to_name);
void fatfs_closefs();

#endif // FATFS_H
