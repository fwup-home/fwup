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

#include "config.h"
#include "cfgprint.h"
#include "fwfile.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <archive_entry.h>
#include <sodium.h>

int fwfile_add_meta_conf(cfg_t *cfg, struct archive *a, const unsigned char *signing_key)
{
    char *configtxt;
    size_t configtxt_len;

    configtxt_len = fwup_cfg_to_string(cfg, &configtxt);
    if (configtxt_len == 0)
        ERR_RETURN("Could not create meta.conf contents");

    int rc = fwfile_add_meta_conf_str(configtxt, configtxt_len, a, signing_key);

    free(configtxt);
    return rc;
}

int fwfile_add_meta_conf_str(const char *configtxt, int configtxt_len,
                             struct archive *a, const unsigned char *signing_key)
{
    struct archive_entry *entry;

    // If the user passed in a signing key, sign the meta.conf.
    if (signing_key) {
        unsigned char signature[crypto_sign_BYTES];
        crypto_sign_detached(signature, NULL,
                             (unsigned char *) configtxt, configtxt_len,
                             signing_key);

        entry = archive_entry_new();
        archive_entry_set_pathname(entry, "meta.conf.ed25519");
        archive_entry_set_size(entry, sizeof(signature));
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(a, entry);
        archive_write_data(a, signature, sizeof(signature));
        archive_entry_free(entry);
    }

    // Add meta.conf
    entry = archive_entry_new();
    archive_entry_set_pathname(entry, "meta.conf");
    archive_entry_set_size(entry, configtxt_len);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, configtxt, configtxt_len);
    archive_entry_free(entry);

    return 0;
}

static int calculate_total_filesize(const char *local_paths, off_t *len)
{
    int rc = 0;
    char *paths = strdup(local_paths);

    *len = 0;
    for (char *path = strtok(paths, ";");
         path != NULL;
         path = strtok(NULL, ";")) {
        FILE *fp = fopen(path, "rb");
        if (!fp)
            ERR_CLEANUP_MSG("can't open '%s'", path);

        fseeko(fp, 0, SEEK_END);
        off_t file_len = ftello(fp);
        fseeko(fp, 0, SEEK_SET);
        fclose(fp);

        *len += file_len;
    }

cleanup:
    free(paths);
    return rc;
}

int fwfile_add_local_file(struct archive *a,
                          const char *resource_name,
                          const char *local_paths,
                          const struct fwfile_assertions *assertions)
{
    int rc = 0;

    off_t copy_buffer_len = 64 * 1024;
    char *copy_buffer = (char *) malloc(copy_buffer_len);
    struct archive_entry *entry = archive_entry_new();
    off_t total_read = 0;
    char *paths = strdup(local_paths);
    FILE *fp = NULL;

    if (*paths == '\0')
        ERR_CLEANUP_MSG("must specify a host-path for resource '%s'", resource_name);

    off_t total_len;
    if (calculate_total_filesize(local_paths, &total_len) < 0)
        goto cleanup; // Error set by calculate_total_filesize()

    if (assertions) {
        if (assertions->assert_gte >= 0 &&
                !(total_len >= assertions->assert_gte))
            ERR_CLEANUP_MSG("file size assertion failed on '%s'. Size must be >= %d bytes (%d blocks)",
                            local_paths, assertions->assert_gte, assertions->assert_gte / 512);
        if (assertions->assert_lte >= 0 &&
                !(total_len <= assertions->assert_lte))
            ERR_CLEANUP_MSG("file size assertion failed on '%s'. Size must be <= %d bytes (%d blocks)",
                            local_paths, assertions->assert_lte, assertions->assert_lte / 512);
    }

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

    for (char *path = strtok(paths, ";");
         path != NULL;
         path = strtok(NULL, ";")) {
        fp = fopen(path, "rb");
        if (!fp)
            ERR_CLEANUP_MSG("can't open '%s'", path);

        size_t len = fread(copy_buffer, 1, (size_t) copy_buffer_len, fp);
        off_t file_read = (off_t) len;
        while (len > 0) {
            off_t written = archive_write_data(a, copy_buffer, len);
            if (written != (off_t) len)
                ERR_CLEANUP_MSG("error writing to archive");

            len = fread(copy_buffer, 1, copy_buffer_len, fp);
            file_read += len;
        }
        total_read += file_read;
    }
    if (total_read != total_len)
        ERR_CLEANUP_MSG("read error for '%s'", paths);

cleanup:
    archive_entry_free(entry);
    if (fp)
        fclose(fp);

    free(copy_buffer);
    free(paths);

    return rc;
}
