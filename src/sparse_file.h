/*
 * Copyright 2016-2017 Frank Hunleth
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

#ifndef SPARSE_FILE_H
#define SPARSE_FILE_H

#include <confuse.h>
#include <sys/types.h>

struct sparse_file_map
{
    // A pointer to the map. Entries alternate between data segments and
    // holes. For example, entry 0 is the number of bytes in the first
    // data segment. If the file starts with a hole, then this is 0. Entry
    // 1 is the number of bytes in the first hole, and so on.
    off_t *map;

    // This is the number of entries in the map.
    int map_len;
};

struct sparse_file_read_iterator
{
    const struct sparse_file_map *sfm;
    int map_ix;
    off_t offset_in_segment;
};

// Only support so many data/hole fragments in a file. After
// this amount, everything is merged together in one big data
// fragment.
#define SPARSE_FILE_MAP_MAX_LEN    256

void sparse_file_init(struct sparse_file_map *sfm);
void sparse_file_free(struct sparse_file_map *sfm);

int sparse_file_get_map_from_config(cfg_t *cfg, const char *resource_name, struct sparse_file_map *sfm);
int sparse_file_get_map_from_resource(cfg_t *resource, struct sparse_file_map *sfm);
int sparse_file_set_map_in_resource(cfg_t *resource, const struct sparse_file_map *sfm);

int sparse_file_build_map_from_fd(int fd, struct sparse_file_map *sfm);


off_t sparse_file_size(const struct sparse_file_map *sfm);
off_t sparse_file_data_size(const struct sparse_file_map *sfm);
off_t sparse_ending_hole_size(const struct sparse_file_map *sfm);

void sparse_file_start_read(const struct sparse_file_map *sfm, struct sparse_file_read_iterator *iterator);
int sparse_file_read_next_data(struct sparse_file_read_iterator *iterator, int fd, off_t *offset, void *buf, size_t buf_len, size_t *len);

int sparse_file_is_supported(const char *testfile, size_t min_hole_size);

#endif // SPARSE_FILE_H
