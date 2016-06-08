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

#include "fwup_create.h"
#include "cfgfile.h"
#include "util.h"
#include "fwfile.h"
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <archive.h>
#include <archive_entry.h>
#include <sodium.h>

static int compute_file_metadata(cfg_t *cfg)
{
    cfg_t *sec;
    int i = 0;

    while ((sec = cfg_getnsec(cfg, "file-resource", i++)) != NULL) {
        const char *paths = cfg_getstr(sec, "host-path");
        if (!paths)
            ERR_RETURN("host-path must be set for file-resource");

        crypto_generichash_state hash_state;
        crypto_generichash_init(&hash_state, NULL, 0, crypto_generichash_BYTES);

        size_t total = 0;
        char *paths_copy = strdup(paths);
        for (char *path = strtok(paths_copy, ";");
             path != NULL;
             path = strtok(NULL, ";")) {
            FILE *fp = fopen(path, "rb");
            if (!fp) {
                free(paths_copy);
                ERR_RETURN("can't open file-resource '%s'", path);
            }

            char buffer[1024];
            size_t len = fread(buffer, 1, sizeof(buffer), fp);
            while (len > 0) {
                crypto_generichash_update(&hash_state, (unsigned char*) buffer, len);
                total += len;
                len = fread(buffer, 1, sizeof(buffer), fp);
            }
            fclose(fp);
        }
        free(paths_copy);

        unsigned char hash[crypto_generichash_BYTES];
        crypto_generichash_final(&hash_state, hash, sizeof(hash));
        char hash_str[sizeof(hash) * 2 + 1];
        bytes_to_hex(hash, hash_str, sizeof(hash));

        cfg_setstr(sec, "blake2b-256", hash_str);
        cfg_setint(sec, "length", total);
    }

    return 0;
}

static int add_file_resources(cfg_t *cfg, struct archive *a)
{
    cfg_t *sec;
    int i = 0;

    while ((sec = cfg_getnsec(cfg, "file-resource", i++)) != NULL) {
        const char *hostpath = cfg_getstr(sec, "host-path");
        if (!hostpath)
            ERR_RETURN("specify a host-path");

        struct fwfile_assertions assertions;
        assertions.assert_lte = cfg_getint(sec, "assert-size-lte") * 512;
        assertions.assert_gte = cfg_getint(sec, "assert-size-gte") * 512;

        OK_OR_RETURN(fwfile_add_local_file(a, cfg_title(sec), hostpath, &assertions));
    }

    return 0;
}

static int create_archive(cfg_t *cfg, const char *filename, const unsigned char *signing_key)
{
    int rc = 0;
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    if (archive_write_open_filename(a, filename) != ARCHIVE_OK)
        ERR_CLEANUP_MSG("error creating archive");

    OK_OR_CLEANUP(fwfile_add_meta_conf(cfg, a, signing_key));

    OK_OR_CLEANUP(add_file_resources(cfg, a));

cleanup:
    archive_write_close(a);
    archive_write_free(a);

    return rc;
}

int fwup_create(const char *configfile, const char *output_firmware, const unsigned char *signing_key)
{
    cfg_t *cfg = NULL;
    int rc = 0;

    // Parse configuration
    OK_OR_CLEANUP(cfgfile_parse_file(configfile, &cfg));

    // Automatically add fwup metadata
    cfg_setstr(cfg, "meta-creation-date", get_creation_timestamp());
    cfg_setstr(cfg, "meta-fwup-version", PACKAGE_VERSION);

    // Compute all metadata
    OK_OR_CLEANUP(compute_file_metadata(cfg));

    // Create the archive
    OK_OR_CLEANUP(create_archive(cfg, output_firmware, signing_key));

cleanup:
    if (cfg)
        cfgfile_free(cfg);

    return rc;
}
