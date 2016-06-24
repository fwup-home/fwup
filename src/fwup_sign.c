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

#include "fwup_sign.h"
#include "fwfile.h"
#include "util.h"
#include "cfgfile.h"
#include "archive_open.h"

#include <archive.h>
#include <archive_entry.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * @brief Sign a firmware update file
 * @param input_filename the firmware update filename
 * @param output_filename where to store the signed firmware update
 * @param signing_key the signing key
 * @return 0 if successful
 */
int fwup_sign(const char *input_filename, const char *output_filename, const unsigned char *signing_key)
{
    int rc = 0;
    char *configtxt = NULL;
    char buffer[4096];
    char *temp_filename = NULL;

    struct archive *in = archive_read_new();
    archive_read_support_format_zip(in);
    struct archive *out = archive_write_new();
    archive_write_set_format_zip(out);

    if (!input_filename)
        ERR_CLEANUP_MSG("Specify an input firmware file");
    if (!output_filename)
        ERR_CLEANUP_MSG("Specify an output firmware file");
    if (!signing_key)
        ERR_CLEANUP_MSG("Specify a signing key");

    temp_filename = malloc(strlen(input_filename) + 5);
    if (!temp_filename)
        ERR_CLEANUP_MSG("Out of memory");
    strcpy(temp_filename, input_filename);
    strcat(temp_filename, ".tmp");

    rc = fwup_archive_open_filename(in, input_filename);
    if (rc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("Cannot open archive '%s'", input_filename);

    if (archive_write_open_filename(out, temp_filename) != ARCHIVE_OK)
        ERR_CLEANUP_MSG("error creating archive");

    struct archive_entry *in_ae;
    while (archive_read_next_header(in, &in_ae) == ARCHIVE_OK) {
        if (strcmp(archive_entry_pathname(in_ae), "meta.conf.ed25519") == 0) {
            // Skip old signature
        } else if (strcmp(archive_entry_pathname(in_ae), "meta.conf") == 0) {
            if (configtxt)
                ERR_CLEANUP_MSG("Invalid firmware. More than one meta.conf found");

            off_t configtxt_len;
            if (archive_read_all_data(in, in_ae, &configtxt, 50000, &configtxt_len) < 0)
                ERR_CLEANUP_MSG("Error reading meta.conf from archive.");

            if (configtxt_len < 10 || configtxt_len >= 50000)
                ERR_CLEANUP_MSG("Unexpected meta.conf size: %d", configtxt_len);

            OK_OR_CLEANUP(fwfile_add_meta_conf_str(configtxt, configtxt_len, out, signing_key));
        } else {
            if (!configtxt)
                ERR_CLEANUP_MSG("Invalid firmware. meta.conf must be at the beginning of archive");

            // Copy the file
            rc = archive_write_header(out, in_ae);
            if (rc != ARCHIVE_OK)
                ERR_CLEANUP_MSG("Error writing '%s' header to '%s'", archive_entry_pathname(in_ae), temp_filename);

            ssize_t size_left = archive_entry_size(in_ae);
            while (size_left > 0) {
                ssize_t to_read = sizeof(buffer);
                if (to_read > size_left)
                    to_read = size_left;

                ssize_t len = archive_read_data(in, buffer, to_read);
                if (len <= 0)
                    ERR_CLEANUP_MSG("Error reading '%s' in '%s'", archive_entry_pathname(in_ae), input_filename);

                if (archive_write_data(out, buffer, len) != len)
                    ERR_CLEANUP_MSG("Error writing '%s' to '%s'", archive_entry_pathname(in_ae), temp_filename);

                size_left -= len;
            }
        }
    }

    if (!configtxt)
        ERR_CLEANUP_MSG("Invalid firmware. No meta.conf not found");

    if (rename(temp_filename, output_filename) < 0)
        ERR_CLEANUP_MSG("Error updating '%s'", input_filename);
    free(temp_filename);
    temp_filename = NULL;

cleanup:
    archive_write_close(out);
    archive_write_free(out);
    archive_read_close(in);
    archive_read_free(in);

    // Only unlink the temporary file if something failed.
    if (temp_filename) {
        unlink(temp_filename);
        free(temp_filename);
    }
    if (configtxt)
        free(configtxt);

    return rc;

}
