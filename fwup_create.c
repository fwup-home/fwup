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
#include "sha2.h"

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <archive.h>
#include <archive_entry.h>

static void compute_file_metadata(cfg_t *cfg)
{
    cfg_t *sec;
    int i = 0;

    while ((sec = cfg_getnsec(cfg, "file-resource", i++)) != NULL) {
        const char *path = cfg_getstr(sec, "host-path");
        if (!path)
            errx(EXIT_FAILURE, "host-path must be set for file-report '%s'", cfg_title(sec));

        FILE *fp = fopen(path, "rb");
        if (!fp)
            err(EXIT_FAILURE, "Cannot open '%s'", path);

        SHA256_CTX ctx256;
        char buffer[1024];
        size_t len = fread(buffer, 1, sizeof(buffer), fp);
        size_t total = len;
        while (len > 0) {
            SHA256_Update(&ctx256, (unsigned char*) buffer, len);
            total += len;
            len = fread(buffer, 1, sizeof(buffer), fp);
        }
        char digest[SHA256_DIGEST_STRING_LENGTH];
        SHA256_End(&ctx256, digest);

        cfg_setstr(sec, "sha256", digest);
        cfg_setint(sec, "length", total);
    }
}

static void cfg_to_string(cfg_t *cfg, char **output, size_t *len)
{
    FILE *fp = open_memstream(output, len);
    cfg_print(cfg, fp);
    fclose(fp);
}

static void add_meta_conf(cfg_t *cfg, struct archive *a)
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
}

static void add_file_resources(cfg_t *cfg, struct archive *a)
{
    cfg_t *sec;
    int i = 0;

    size_t buffer_len = 64 * 1024;
    char *buffer = (char *) malloc(buffer_len);

    while ((sec = cfg_getnsec(cfg, "file-resource", i++)) != NULL) {
        const char *hostpath = cfg_getstr(sec, "host-path");
        if (!hostpath)
            errx(EXIT_FAILURE, "host-path must be set for file-report '%s'", cfg_title(sec));

        FILE *fp = fopen(hostpath, "rb");
        if (!fp)
            err(EXIT_FAILURE, "Cannot open '%s'", hostpath);

        size_t total_len = cfg_getint(sec, "length");
        struct archive_entry *entry = archive_entry_new();

        char destpath[256];
        sprintf(destpath, "data/%s", cfg_title(sec));
        archive_entry_set_pathname(entry, destpath);
        archive_entry_set_size(entry, total_len);
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(a, entry);

        size_t len = fread(buffer, 1, buffer_len, fp);
        while (len > 0) {
            archive_write_data(a, buffer, len);
            len = fread(buffer, 1, buffer_len, fp);
        }
        archive_entry_free(entry);
    }
    free(buffer);
}

static int create_archive(cfg_t *cfg, const char *filename)
{
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    if (archive_write_open_filename(a, filename) != ARCHIVE_OK)
        return -1;

    add_meta_conf(cfg, a);

    add_file_resources(cfg, a);

    archive_write_close(a);
    archive_write_free(a);

    return 0;
}

void fwup_create(const char *configfile, const char *output_firmware)
{
    cfg_t *cfg;

    set_now_time();

    // Parse configuration
    if (cfgfile_parse_file(configfile, &cfg) < 0)
        errx(EXIT_FAILURE, "Error parsing '%s'", configfile);

    // Compute all metadata
    compute_file_metadata(cfg);

    // Create the archive
    if (create_archive(cfg, output_firmware) < 0)
        errx(EXIT_FAILURE, "Errore creating archive '%s'", output_firmware);

    cfgfile_free(cfg);
}
