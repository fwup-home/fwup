/*
 * Copyright 2016 Frank Hunleth
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

#include "sparse_file.h"
#include "util.h"

#include <stdlib.h>

/**
 * @brief Read a sparse map out of the config
 *
 * The sparse map is an integer list where the first element is the
 * length of the first data block, the second is the length of the
 * first hole, the third is the length of the second data block, and
 * so on. If a file isn't sparse, the list has only one element and
 * it's the length of the file.
 *
 * @param cfg
 * @param resource_name
 * @param sparse_map
 * @param sparse_map_len
 * @return 0 if successful
 */
int sparse_file_get_map_from_config(cfg_t *cfg, const char *resource_name, off_t **sparse_map, int *sparse_map_len)
{

    cfg_t *resource = cfg_gettsec(cfg, "file-resource", resource_name);
    if (!resource)
        ERR_RETURN("file-resource '%s' not found", resource_name);

    int len = cfg_size(resource, "length");
    if (len <= 0) {
        // If not found, then libconfuse supplies the default value of 0
        // for the first element. I.e., this is a 0 length file.
        len = 1;
    }

    off_t *map = (off_t *) malloc(len * sizeof(off_t));
    for (int i = 0; i < len; i++)
        map[i] = cfg_getnint(resource, "length", i);

    *sparse_map = map;
    *sparse_map_len = len;
    return 0;
}
