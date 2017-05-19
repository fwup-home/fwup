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

#include "config.h"

#if HAVE_SPARSE_SEEK
#define _GNU_SOURCE // for SEEK_DATA and SEEK_HOLE
#endif

#include "sparse_file.h"
#include "util.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// Helper functions to determine whether a sparse map is a
// hole or data -> Even offsets are data; odd ones are holes
#define IN_HOLE(ix) (((ix) & 1) != 0)

/**
 * @brief Initialize the sparse_file_map data structure
 * @param sfm
 */
void sparse_file_init(struct sparse_file_map *sfm)
{
    sfm->map = NULL;
    sfm->map_len = 0;
}

/**
 * @brief Free the data associated with a sparse file map
 * @param sfm
 */
void sparse_file_free(struct sparse_file_map *sfm)
{
    if (sfm->map)
        free(sfm->map);
    sfm->map = NULL;
    sfm->map_len = 0;
}

/**
 * @brief Read a sparse map out of the config
 *
 * The sparse map is an integer list where the first element is the
 * length of the first data block, the second is the length of the
 * first hole, the third is the length of the second data block, and
 * so on. If a file isn't sparse, the list has only one element and
 * it's the length of the file.
 *
 * @param cfg the main config
 * @param resource_name which resource
 * @param sfm an initialized sparse file map is returned
 * @return 0 if successful
 */
int sparse_file_get_map_from_config(cfg_t *cfg, const char *resource_name, struct sparse_file_map *sfm)
{
    cfg_t *resource = cfg_gettsec(cfg, "file-resource", resource_name);
    if (!resource)
        ERR_RETURN("file-resource '%s' not found", resource_name);

    return sparse_file_get_map_from_resource(resource, sfm);
}

/**
 * @brief Read a sparse map from a resource config
 *
 * See spares_file_get_map_from_config() for docs.
 *
 * @param resource the cfg_t * to the resource
 * @param sfm where to store the map
 * @return 0 if successful
 */
int sparse_file_get_map_from_resource(cfg_t *resource, struct sparse_file_map *sfm)
{
    // If this map was in use, free any memory associated with it.
    sparse_file_free(sfm);

    int map_len = cfg_size(resource, "length");
    if (map_len <= 0) {
        // If not found, then libconfuse supplies the default value of 0
        // for the first element. I.e., this is a 0 length file.
        map_len = 1;
    }

    off_t *map = (off_t *) malloc(map_len * sizeof(off_t));
    for (int i = 0; i < map_len; i++) {
#if (SIZEOF_INT == 4 && SIZEOF_OFF_T > 4)
        // See comment in cfgfile.c to use of doubles to represent
        // file offsets on 32-bit platforms.
        map[i] = (off_t) cfg_getnfloat(resource, "length", i);
#else
        map[i] = cfg_getnint(resource, "length", i);
#endif
    }

    sfm->map = map;
    sfm->map_len = map_len;
    return 0;
}

/**
 * @brief Set the sparse map in the specified resource config
 *
 * @param resource the cft_t * to the resource
 * @param sfm
 * @return
 */
int sparse_file_set_map_in_resource(cfg_t *resource, const struct sparse_file_map *sfm)
{
    for (int i = 0; i < sfm->map_len; i++) {
#if (SIZEOF_INT == 4 && SIZEOF_OFF_T > 4)
        cfg_setnfloat(resource, "length", sfm->map[i], i);
#else
        cfg_setnint(resource, "length", sfm->map[i], i);
#endif
    }

    return 0;
}

/**
 * @brief Return the total size of the sparse file
 *
 * @param sfm the sparse map
 * @return the size is returned
 */
off_t sparse_file_size(const struct sparse_file_map *sfm)
{
    off_t size = 0;
    for (int i = 0; i < sfm->map_len; i++)
        size += sfm->map[i];

    return size;
}

/**
 * @brief Return the data-containing size of the sparse file
 * @param sfm the sparse map
 * @return the size is returned
 */
off_t sparse_file_data_size(const struct sparse_file_map *sfm)
{
    off_t size = 0;
    for (int i = 0; i < sfm->map_len; i += 2)
        size += sfm->map[i];

    return size;
}

/**
 * @brief Returns how many bytes are in the hole at the very end of the file
 * @param sfm the sparse map
 * @return true if ending with a hole; false if not
 */
off_t sparse_ending_hole_size(const struct sparse_file_map *sfm)
{
    // If even length then it ends with a hole, so return it.
    if ((sfm->map_len & 1) == 0)
        return sfm->map[sfm->map_len - 1];
    else
        return 0;
}

/**
 * @brief Compute a sparse map of a file from a file descriptor
 *
 * The fd's position is left at an arbitrary location. This can
 * be called repeatedly to build a sparse map across a bunch of
 * files that will be concatenated.
 *
 * Note that if sparse seeking isn't supported, the map will have
 * only one entry.
 *
 * @param fd a file descriptor with read access
 * @param sfm a location to store the sparse map
 * @return 0 if successful
 */
int sparse_file_build_map_from_fd(int fd, struct sparse_file_map *sfm)
{
    off_t leftover;
    int i;
    if (!sfm->map) {
        // First file -> start fresh
        sfm->map = (off_t *) malloc(SPARSE_FILE_MAP_MAX_LEN * sizeof(off_t));
        sfm->map_len = 0;
        leftover = 0;
        i = 0;
    } else {
        // Continuing to build, so rewind to the last
        // entry, and see if the beginning of this file is the same
        // (data or hole) as the end of the previous file.
        i = sfm->map_len - 1;
        leftover = sfm->map[i];
    }

    off_t offset = 0;
    off_t next = 0;

#if HAVE_SPARSE_SEEK
    for (; i < SPARSE_FILE_MAP_MAX_LEN - 2; i++) {
        next = lseek(fd, offset, IN_HOLE(i) ? SEEK_DATA : SEEK_HOLE);
        if (next < 0) {
            // Normal case -> we hit the end
            next = lseek(fd, 0, SEEK_END);
            if (next != offset)
                sfm->map[i++] = next - offset + leftover;
            sfm->map_len = i;
            return 0;
        }
        if (i >= 1 && offset == next && sfm->map[i - 1] == 0) {
            // Special case where we're in a hole and in data
            // This happens for /dev/zero and possibly other
            // special files.
            sfm->map_len = i;
            return 0;
        }
        sfm->map[i] = next - offset + leftover;
        leftover = 0;
        offset = next;
    }

    // Ran out of entries, so the remainder is one big data segment.
    if (IN_HOLE(i)) {
        // Current in a hole, so advance to an even location for a data fragment
        sfm->map[i] = next - offset + leftover;
        leftover = 0;
        offset = next;
        i++;
    }
#endif
    next = lseek(fd, 0, SEEK_END);
    if (next < 0)
        return -1;

    sfm->map[i] = next - offset + leftover;
    sfm->map_len = i + 1;
    return 0;
}

/**
 * @brief Reset the sparse map iterators to read through a file or set of files
 * @param sfm the sparse map
 */
void sparse_file_start_read(const struct sparse_file_map *sfm,
                            struct sparse_file_read_iterator *iterator)
{
    iterator->sfm = sfm;
    iterator->map_ix = 0;
    iterator->offset_in_segment = 0;
}

/**
 * @brief Read the next block of data according to the sparse_file_map
 *
 * This uses the sparse_file_map information to determine what's a hole
 * and what's not. This usually corresponds to what's on disk, but not
 * always (i.e. too many holes). Since the sparse_file_map is "truth", it
 * has to drive what gets read.
 *
 * @param iterator the sparce file map iterator
 * @param fd      a file descriptor
 * @param offset  where reading should start in fd; next offset is returned
 * @param buf     where to store the data
 * @param buf_len the size of the buf
 * @param len     how many bytes read
 * @return 0 if successful
 */
int sparse_file_read_next_data(struct sparse_file_read_iterator *iterator, int fd, off_t *offset, void *buf, size_t buf_len, size_t *len)
{
    const struct sparse_file_map *sfm = iterator->sfm;

    // Nothing read yet.
    *len = 0;

    // Read past holes and empty blocks
    for (;;) {
        // Check for end of file
        if (iterator->map_ix == sfm->map_len)
            return 0;

        off_t bytes_left_of_segment = sfm->map[iterator->map_ix] - iterator->offset_in_segment;

        // If currently in a hole, advance out of it.
        if (IN_HOLE(iterator->map_ix)) {
            off_t end = lseek(fd, 0, SEEK_END);
            if (end < 0)
                return -1;
            off_t bytes_left_of_file = end - *offset;
            if (bytes_left_of_file < bytes_left_of_segment) {
                // Hole crosses between files
                // Concatenation a file with a hole at the end with on with a hole at the beginning.
                iterator->offset_in_segment += bytes_left_of_file;
                *offset = end;
                return 0;
            }
            *offset += bytes_left_of_segment;
        } else if (bytes_left_of_segment != 0) {
            // Data segment with something to process
            break;
        }

        // Advance to the next segment
        iterator->offset_in_segment = 0;
        iterator->map_ix++;
    };

    // Read as much as we can.
    off_t to_read = sfm->map[iterator->map_ix] - iterator->offset_in_segment;
    if (to_read > (off_t) buf_len)
        to_read = buf_len;

    ssize_t rc = pread(fd, buf, to_read, *offset);
    if (rc < 0)
        return rc;

    iterator->offset_in_segment += rc;
    if (iterator->offset_in_segment == sfm->map[iterator->map_ix]) {
        iterator->offset_in_segment = 0;
        iterator->map_ix++;
    }

    *offset += rc;
    *len = rc;
    return 0;
}

/**
 * @brief Check whether sparse files are supported on a filesystem
 *
 * @param testfile a path to a file to write/read on the desired filesystem to check
 * @param min_hole_size the minimum hole size to check
 * @return 0 if they are
 */

int sparse_file_is_supported(const char *testfile, size_t min_hole_size)
{
#if HAVE_SPARSE_SEEK
    int fd = open(testfile, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (fd < 0)
        ERR_RETURN("Couldn't create sparse test file: %s", testfile);

    // Write something (anything) in the middle of the file.
    // If sparse files are supported, the beginning will be sparse.
    int rc = pwrite(fd, &fd, sizeof(fd), min_hole_size);
    if (rc != sizeof(fd))
        ERR_CLEANUP_MSG("Sparse check write to offset %d failed.", min_hole_size);

    lseek(fd, 0, SEEK_SET);
    off_t offset = lseek(fd, 0, SEEK_DATA);
    if (offset != min_hole_size)
        ERR_CLEANUP_MSG("Hole of %d bytes not created on filesystem", min_hole_size);

    // It worked.
    rc = 0;

cleanup:
    close(fd);
    unlink(testfile);
    return rc;
#else
    (void) testfile;
    (void) min_hole_size;
    // If the OS doesn't support it, then don't bother trying.
    ERR_RETURN("Sparse file supported not compiled into fwup.");
#endif
}
