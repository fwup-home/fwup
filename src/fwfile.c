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

#include "fwfile.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <archive_entry.h>

static void cfg_to_string(cfg_t *cfg, char **output, size_t *len)
{
    FILE *fp = open_memstream(output, len);
    cfg_print(cfg, fp);
    fclose(fp);
}

int fwfile_add_meta_conf(cfg_t *cfg, struct archive *a)
{
    char *configtxt;
    size_t configtxt_len;

    cfg_to_string(cfg, &configtxt, &configtxt_len);

    struct archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname(entry, "meta.conf");
    archive_entry_set_size(entry, configtxt_len);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, configtxt, configtxt_len);
    archive_entry_free(entry);

    free(configtxt);
    return 0;
}

int fwfile_add_local_file(struct archive *a, const char *resource_name, const char *local_path)
{
    int rc = 0;

    size_t copy_buffer_len = 64 * 1024;
    char *copy_buffer = (char *) malloc(copy_buffer_len);
    struct archive_entry *entry = 0;

    FILE *fp = fopen(local_path, "rb");
    if (!fp)
        ERR_CLEANUP_MSG("can't open local file");

    fseek(fp, 0, SEEK_END);
    size_t total_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    entry = archive_entry_new();

    // Convert the resource name to an archive path (most resources should be in the data directory)
    char archive_path[FWFILE_MAX_ARCHIVE_PATH];
    size_t resource_name_len = strlen(resource_name);
    if (resource_name_len + 6 > sizeof(archive_path))
        ERR_CLEANUP_MSG("resource name is too long");
    if (resource_name_len == '\0')
        ERR_CLEANUP_MSG("resource name can't be empty");
    if (resource_name[resource_name_len - 1] == '/')
        ERR_CLEANUP_MSG("resource name can't end in a '/'");

    if (resource_name[0] == '/') {
        if (resource_name[1] == '\0')
            ERR_CLEANUP_MSG("resource name can't be the root directory");

        // This seems like it's just asking for trouble, so error out.
        if (strcmp(resource_name, "/meta.conf") == 0)
            ERR_CLEANUP_MSG("resources can't be named /meta.conf");

        // Absolute paths are not intended to be commonly used and ones
        // in /data won't work when applying the updates, so error out.
        if (memcmp(resource_name, "/data/", 6) == 0 ||
            strcmp(resource_name, "/data") == 0)
            ERR_CLEANUP_MSG("use a normal resource name rather than specifying /data");

        strcpy(archive_path, &resource_name[1]);
    } else {
        sprintf(archive_path, "data/%s", resource_name);
    }
    archive_entry_set_pathname(entry, archive_path);
    archive_entry_set_size(entry, total_len);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);

    size_t len = fread(copy_buffer, 1, copy_buffer_len, fp);
    size_t total_read = len;
    while (len > 0) {
        ssize_t written = archive_write_data(a, copy_buffer, len);
        if (written != (ssize_t) len)
            ERR_CLEANUP_MSG("error writing to archive");

        len = fread(copy_buffer, 1, copy_buffer_len, fp);
        total_read += len;
    }
    if (total_read != total_len)
        ERR_CLEANUP_MSG("read an unexpected amount of data");

cleanup:
    archive_entry_free(entry);
    if (fp)
        fclose(fp);

    free(copy_buffer);

    return rc;
}

