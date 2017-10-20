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

#include "fwup_verify.h"
#include "fwfile.h"
#include "util.h"
#include "cfgfile.h"
#include "archive_open.h"
#include "sparse_file.h"
#include "resources.h"

#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sodium.h>

static int check_resource(struct resource_list *list, const char *file_resource_name, struct archive *a, struct archive_entry *ae)
{
    struct resource_list *item = rlist_find_by_name(list, file_resource_name);
    if (!item)
        ERR_RETURN("Can't find file-resource for %s", file_resource_name);

    if (item->processed)
        ERR_RETURN("Processing %s twice. Archive is corrupt.", file_resource_name);
    item->processed = true;

    struct sparse_file_map sfm;
    sparse_file_init(&sfm);
    OK_OR_RETURN(sparse_file_get_map_from_resource(item->resource, &sfm));

    size_t expected_length = sparse_file_data_size(&sfm);
    ssize_t archive_length = archive_entry_size(ae);
    if (archive_length < 0)
        ERR_RETURN("Missing file length in archive for %s", file_resource_name);
    if ((size_t) archive_length != expected_length)
        ERR_RETURN("Length mismatch for %s", file_resource_name);

    char *expected_hash = cfg_getstr(item->resource, "blake2b-256");
    if (!expected_hash || strlen(expected_hash) != crypto_generichash_BYTES * 2)
        ERR_RETURN("invalid blake2b-256 hash for '%s'", file_resource_name);

    crypto_generichash_state hash_state;
    crypto_generichash_init(&hash_state, NULL, 0, crypto_generichash_BYTES);
    size_t length_left = expected_length;
    while (length_left != 0) {
        char buffer[4096];

        size_t to_read = sizeof(buffer);
        if (to_read > length_left)
            to_read = length_left;

        ssize_t len = archive_read_data(a, buffer, to_read);
        if (len <= 0)
            ERR_RETURN("Error reading '%s' in archive", archive_entry_pathname(ae));

        crypto_generichash_update(&hash_state, (unsigned char*) buffer, len);
        length_left -= len;
    }

    unsigned char hash[crypto_generichash_BYTES];
    crypto_generichash_final(&hash_state, hash, sizeof(hash));
    char hash_str[sizeof(hash) * 2 + 1];
    bytes_to_hex(hash, hash_str, sizeof(hash));
    if (memcmp(hash_str, expected_hash, sizeof(hash_str)) != 0)
        ERR_RETURN("Detected blake2b digest mismatch for %s", file_resource_name);

    return 0;
}


/**
 * @brief Verify that the firmware archive is ok
 * @param input_filename the firmware update filename
 * @param public_key the public key or NULL if no authentication check
 * @return 0 if successful
 */
int fwup_verify(const char *input_filename, const unsigned char *public_key)
{
    unsigned char *meta_conf_signature = NULL;
    struct resource_list *all_resources = NULL;
    cfg_t *cfg = NULL;
    int rc = 0;

    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);

    if (!input_filename)
        ERR_CLEANUP_MSG("Specify an input firmware file");

    rc = fwup_archive_open_filename(a, input_filename);
    if (rc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("%s", archive_error_string(a));

    struct archive_entry *ae;
    rc = archive_read_next_header(a, &ae);
    if (rc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("%s", archive_error_string(a));

    if (strcmp(archive_entry_pathname(ae), "meta.conf.ed25519") == 0) {
        off_t total_size;
        if (archive_read_all_data(a, ae, (char **) &meta_conf_signature, crypto_sign_BYTES, &total_size) < 0)
            ERR_CLEANUP_MSG("Error reading meta.conf.ed25519 from archive.\n"
                            "Check for file corruption or libarchive built without zlib support");

        if (total_size != crypto_sign_BYTES)
            ERR_CLEANUP_MSG("Unexpected meta.conf.ed25519 size: %d", total_size);

        rc = archive_read_next_header(a, &ae);
        if (rc != ARCHIVE_OK)
            ERR_CLEANUP_MSG("Expecting more than meta.conf.ed25519 in archive");
    }
    if (strcmp(archive_entry_pathname(ae), "meta.conf") != 0)
        ERR_CLEANUP_MSG("Expecting meta.conf to be at the beginning of %s", input_filename);

    OK_OR_CLEANUP(cfgfile_parse_fw_ae(a, ae, &cfg, meta_conf_signature, public_key));

    OK_OR_CLEANUP(rlist_get_all(cfg, &all_resources));

    while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(ae);
        char resource_name[FWFILE_MAX_ARCHIVE_PATH];

        OK_OR_CLEANUP(archive_filename_to_resource(filename, resource_name, sizeof(resource_name)));
        OK_OR_CLEANUP(check_resource(all_resources, resource_name, a, ae));
    }

    // Check that all resources have been validated
    for (struct resource_list *r = all_resources; r != NULL; r = r->next) {
        if (!r->processed)
            ERR_CLEANUP_MSG("Resource %s not found in archive", cfg_title(r->resource));
    }

    if (public_key && meta_conf_signature)
        fwup_warnx("Signed archive '%s' passes signature verification and is not corrupt.", input_filename);
    else if (!public_key && meta_conf_signature)
        fwup_warnx("Signed archive '%s' is not corrupt. Pass a public key to verify the signature.", input_filename);
    else
        fwup_warnx("Unsigned archive '%s' is not corrupt.", input_filename);

cleanup:
    rlist_free(all_resources);
    archive_read_close(a);
    archive_read_free(a);

    if (meta_conf_signature)
        free(meta_conf_signature);

    if (cfg)
        cfgfile_free(cfg);

    return rc;
}
