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

#ifndef FATFS_H
#define FATFS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/*
 * API wrappers to make the FatFS library easier to use.
 */

struct block_cache;

struct tm;
int fatfs_set_time(struct tm *tmp);

int fatfs_mkfs(struct block_cache *output, off_t block_offset, size_t block_count);
int fatfs_attrib(struct block_cache *output, off_t block_offset, const char *filename, const char *attrib);
int fatfs_mkdir(struct block_cache *output, off_t block_offset, const char *dir);
int fatfs_setlabel(struct block_cache *output, off_t block_offset, const char *label);
int fatfs_mv(struct block_cache *output, off_t block_offset, const char *cmd, const char *from_name, const char *to_name, bool force);
int fatfs_rm(struct block_cache *output, off_t block_offset, const char *cmd, const char *filename, bool file_must_exist);
int fatfs_truncate(struct block_cache *output, off_t block_offset, const char *filename);
int fatfs_pwrite(struct block_cache *output, off_t block_offset, const char *filename, int offset, const char *buffer, off_t size);
int fatfs_cp(struct block_cache *output, off_t block_offset, const char *from_name, const char *to_name);
int fatfs_touch(struct block_cache *output, off_t block_offset, const char *filename);
int fatfs_exists(struct block_cache *output, off_t block_offset, const char *filename);
int fatfs_file_matches(struct block_cache *output, off_t block_offset, const char *filename, const char *pattern);
void fatfs_closefs();

#endif // FATFS_H
