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

#include "fwup_verify.h"
#include "fwfile.h"
#include "util.h"
#include "cfgfile.h"

#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sodium.h>

static int check_resource(cfg_t *cfg, const char *file_resource_name, struct archive *a, struct archive_entry *ae)
{
    cfg_t *resource = cfg_gettsec(cfg, "file-resource", file_resource_name);
    if (!resource)
        ERR_RETURN("Can't find file-resource for %s", file_resource_name);
    size_t expected_length = cfg_getint(resource, "length");
    ssize_t archive_length = archive_entry_size(ae);
    if (archive_length < 0)
        ERR_RETURN("Missing file length in archive for %s", file_resource_name);
    if ((size_t) archive_length != expected_length)
        ERR_RETURN("Length mismatch for %s", file_resource_name);

    char *expected_hash = cfg_getstr(resource, "blake2b-256");
    if (strlen(expected_hash) != crypto_generichash_BYTES * 2)
        ERR_RETURN("Detected blake2b hash with the wrong length for %s", file_resource_name);

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
    cfg_t *cfg = NULL;
    int rc = 0;

    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);

    if (!input_filename)
        ERR_CLEANUP_MSG("Specify an input firmware file");

    rc = archive_read_open_filename(a, input_filename, 16384);
    if (rc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("Cannot open archive '%s'", input_filename);

    struct archive_entry *ae;
    rc = archive_read_next_header(a, &ae);
    if (rc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("Error reading archive");

    if (strcmp(archive_entry_pathname(ae), "meta.conf.ed25519") == 0) {
        ssize_t total_size = archive_entry_size(ae);
        if (total_size != crypto_sign_BYTES)
            ERR_CLEANUP_MSG("Unexpected meta.conf.ed25519 size: %d", total_size);

        meta_conf_signature = (unsigned char *) malloc(total_size);
        if (archive_read_all_data(a, (char *) meta_conf_signature, total_size) < 0)
            ERR_CLEANUP_MSG("Error reading meta.conf.ed25519 from archive.\n"
                            "Check for file corruption or libarchive built without zlib support");

        rc = archive_read_next_header(a, &ae);
        if (rc != ARCHIVE_OK)
            ERR_CLEANUP_MSG("Expecting more than meta.conf.ed25519 in archive");
    }
    if (strcmp(archive_entry_pathname(ae), "meta.conf") != 0)
        ERR_CLEANUP_MSG("Expecting meta.conf to be at the beginning of %s", input_filename);

    OK_OR_CLEANUP(cfgfile_parse_fw_ae(a, ae, &cfg, meta_conf_signature, public_key));

    while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(ae);
        char resource_name[FWFILE_MAX_ARCHIVE_PATH];

        OK_OR_CLEANUP(archive_filename_to_resource(filename, resource_name, sizeof(resource_name)));
        OK_OR_CLEANUP(check_resource(cfg, resource_name, a, ae));
    }

cleanup:
    archive_read_close(a);
    archive_read_free(a);

    if (meta_conf_signature)
        free(meta_conf_signature);

    if (cfg)
        cfgfile_free(cfg);

    return rc;
}

