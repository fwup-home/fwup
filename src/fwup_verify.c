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
#include "monocypher.h"
#include "fwup_xdelta3.h"

#define VERIFICATION_CHUNK_SIZE (64 * 1024)

static int process_entry(struct archive *a, struct archive_entry *ae, off_t *length_read, uint8_t *hash)
{
    crypto_blake2b_ctx hash_state;
    crypto_blake2b_general_init(&hash_state, FWUP_BLAKE2b_256_LEN, NULL, 0);

    *length_read = 0;
    int64_t expected_offset = 0;
    for (;;) {
        // See fwup_apply for comments. This code is intentionally the same
        // to test similar code paths.
        const void *buffer;
        size_t len;
        int64_t offset64;
        int rc = fwup_archive_read_data_block(a, &buffer, &len, &offset64);

        if (rc == ARCHIVE_EOF)
            break;
        else if (rc != ARCHIVE_OK)
            ERR_RETURN(archive_error_string(a));

        if (offset64 != expected_offset)
            ERR_RETURN("Unexpected offset hole when decoding archive");
        expected_offset += len;

        crypto_blake2b_update(&hash_state, (const uint8_t*) buffer, len);
        *length_read += len;
    }

    crypto_blake2b_final(&hash_state, hash);
    return 0;
}

static int get_expected_hash(struct resource_list *item, const char *file_resource_name, const char **expected_hash)
{
    const char *hash = cfg_getstr(item->resource, "blake2b-256");
    if (!hash || strlen(hash) != FWUP_BLAKE2b_256_LEN * 2)
        ERR_RETURN("invalid blake2b-256 hash for '%s'", file_resource_name);

    *expected_hash = hash;

    return 0;
}

static int check_normal_resource(struct resource_list *item,
                                 const char *file_resource_name,
                                 struct archive *a,
                                 struct archive_entry *ae,
                                 off_t expected_length)
{
    uint8_t hash[FWUP_BLAKE2b_256_LEN];
    off_t length_read;

    OK_OR_RETURN(process_entry(a, ae, &length_read, hash));

    if (length_read != expected_length)
        ERR_RETURN("ZIP data length mismatch for %s", file_resource_name);

    const char *expected_hash;
    OK_OR_RETURN(get_expected_hash(item, file_resource_name, &expected_hash));

    char hash_str[sizeof(hash) * 2 + 1];
    bytes_to_hex(hash, hash_str, sizeof(hash));
    if (memcmp(hash_str, expected_hash, sizeof(hash_str)) != 0)
        ERR_RETURN("Detected blake2b digest mismatch for %s", file_resource_name);

    return 0;
}

static int xdelta_read_patch_callback(void* cookie, const void **buffer, size_t *len)
{
    struct archive *a = (struct archive *) cookie;

    int64_t ignored;
    int rc = fwup_archive_read_data_block(a, buffer, len, &ignored);

    if (rc == ARCHIVE_OK) {
        return 0;
    } else if (rc == ARCHIVE_EOF) {
        *len = 0;
        *buffer = NULL;
        return 0;
    } else {
        *len = 0;
        *buffer = NULL;
        ERR_RETURN(archive_error_string(a));
    }
}

static int check_xdelta3_resource(struct resource_list *item, const char *file_resource_name, struct archive *a, struct archive_entry *ae)
{
    // Since verification doesn't have access to the "before" image, we can't run
    // xdelta3 and check that the outcome matches the expected. However, we can
    // do a bunch of sanity checks.

    // Check that there's a Blake 2B hash.
    const char *expected_hash;
    OK_OR_RETURN(get_expected_hash(item, file_resource_name, &expected_hash));

    struct xdelta_state xd;
    xdelta_init(&xd, xdelta_read_patch_callback, NULL, a);

    // This will check that the data at least looks like an xdelta3 patch and the
    // options in the header look decodeable. (xdelta3 will return errors)
    OK_OR_RETURN(xdelta_read_header(&xd));

    xdelta_free(&xd);
    return 0;
}

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

    int sparse_segments = sfm.map_len;
    off_t expected_length = sparse_file_data_size(&sfm);

    sparse_file_free(&sfm);

    off_t archive_length = archive_entry_size(ae);
    if (archive_length < 0)
        ERR_RETURN("Missing file length in archive for %s", file_resource_name);

    if (sparse_segments == 1 && archive_entry_size_is_set(ae) && archive_length != expected_length) {
        // Possible xdelta3 patch
        return check_xdelta3_resource(item, file_resource_name, a, ae);
    } else {
        // Normal file

        if (archive_entry_size_is_set(ae) && archive_length != expected_length)
            ERR_RETURN("ZIP local header length mismatch for %s", file_resource_name);

        return check_normal_resource(item, file_resource_name, a, ae, expected_length);
    }
}

/**
 * @brief Verify that the firmware archive is ok
 * @param input_filename the firmware update filename
 * @param public_keys the public keys if authentication check
 * @return 0 if successful
 */
int fwup_verify(const char *input_filename, unsigned char * const *public_keys)
{
    unsigned char *meta_conf_signature = NULL;
    struct resource_list *all_resources = NULL;
    cfg_t *cfg = NULL;
    int rc = 0;

    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);

    if (!input_filename)
        ERR_CLEANUP_MSG("Specify an input firmware file");

    rc = fwup_archive_open_filename(a, input_filename, NULL);
    if (rc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("%s", archive_error_string(a));

    struct archive_entry *ae;
    rc = archive_read_next_header(a, &ae);
    if (rc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("%s", archive_error_string(a));

    if (strcmp(archive_entry_pathname(ae), "meta.conf.ed25519") == 0) {
        off_t total_size;
        if (archive_read_all_data(a, ae, (char **) &meta_conf_signature, FWUP_SIGNATURE_LEN, &total_size) < 0)
            ERR_CLEANUP_MSG("Error reading meta.conf.ed25519 from archive.\n"
                            "Check for file corruption or libarchive built without zlib support");

        if (total_size != FWUP_SIGNATURE_LEN)
            ERR_CLEANUP_MSG("Unexpected meta.conf.ed25519 size: %d", total_size);

        rc = archive_read_next_header(a, &ae);
        if (rc != ARCHIVE_OK)
            ERR_CLEANUP_MSG("Expecting more than meta.conf.ed25519 in archive");
    }
    if (strcmp(archive_entry_pathname(ae), "meta.conf") != 0)
        ERR_CLEANUP_MSG("Expecting meta.conf to be at the beginning of %s", input_filename);

    OK_OR_CLEANUP(cfgfile_parse_fw_ae(a, ae, &cfg, meta_conf_signature, public_keys));

    OK_OR_CLEANUP(rlist_get_all(cfg, &all_resources));

    while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(ae);
        char resource_name[FWFILE_MAX_ARCHIVE_PATH];
        OK_OR_CLEANUP(archive_filename_to_resource(filename, resource_name, sizeof(resource_name)));

        // Skip empty filenames. These are easy to get when you manually run 'zip'.
        if (resource_name[0] != '\0')
            OK_OR_CLEANUP(check_resource(all_resources, resource_name, a, ae));
    }

    // Check that all resources have been validated
    for (struct resource_list *r = all_resources; r != NULL; r = r->next) {
        if (!r->processed)
            ERR_CLEANUP_MSG("Resource %s not found in archive", cfg_title(r->resource));
    }

    const char *success_message;
    if (*public_keys && meta_conf_signature)
        success_message = "Valid archive with a good signature\n";
    else if (!*public_keys && meta_conf_signature)
        success_message = "Valid archive with an unverified signature. Specify a public key to authenticate.\n";
    else
        success_message = "Valid archive without a signature\n";

    fwup_output(FRAMING_TYPE_SUCCESS, 0, success_message);

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
