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

#include "fwup_apply.h"
#include "util.h"
#include "cfgfile.h"

#include <archive.h>
#include <archive_entry.h>
#include <confuse.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "functions.h"
#include "fatfs.h"

static bool task_is_applicable(cfg_t *task)
{
    // TODO - check that the parameters set on the task are applicable to the current setup.
    (void) task;

    return true;
}

static cfg_t *find_task(cfg_t *cfg, const char *task_prefix)
{
    size_t task_len = strlen(task_prefix);
    cfg_t *task;

    int i;    
    for (i = 0; (task = cfg_getnsec(cfg, "task", i)) != NULL; i++) {
        const char *name = cfg_title(task);
        if (strlen(name) >= task_len &&
                memcmp(task_prefix, name, task_len) == 0 &&
                task_is_applicable(task))
            return task;
    }
    return 0;
}

static int run_event(struct fun_context *fctx, cfg_t *task, const char *event_type, const char *event_parameter)
{
    cfg_t *on_event;
    if (event_parameter)
        on_event = cfg_gettsec(task, event_type, event_parameter);
    else
        on_event = cfg_getsec(task, event_type);

    if (on_event) {
        cfg_opt_t *funlist = cfg_getopt(on_event, "funlist");
        if (funlist) {
            if (fun_run_funlist(fctx, funlist) < 0)
                return -1;
        }
    }
    return 0;
}

struct fwup_data
{
    struct archive *a;

    FILE *fatfp;
    int fatfp_base_offset;
    int64_t current_fatfs_block_offset;
    char *fatfs_buffer;
    size_t fatfs_size;

    struct archive *subarchive;
    char *subarchive_path;
};

static int read_callback(struct fun_context *fctx, const void **buffer, size_t *len, int64_t *offset)
{
    struct fwup_data *p = (struct fwup_data *) fctx->cookie;
    int rc = archive_read_data_block(p->a, buffer, len, offset);
    if (rc == ARCHIVE_EOF) {
        *len = 0;
        *buffer = NULL;
        *offset = 0;
        return 0;
    } else if (rc == ARCHIVE_OK) {
        return 0;
    } else
        ERR_RETURN(archive_error_string(p->a));
}

static int fatfs_ptr_callback(struct fun_context *fctx, int64_t block_offset, bool going_to_call_mkfs, FILE **fatfp, size_t *fatfp_offset)
{
    struct fwup_data *p = (struct fwup_data *) fctx->cookie;

    // Check if this is the first time or if block offset changed or
    // if we're going to call mkfs and nuke everything anyway.
    if (!p->fatfp || block_offset != p->current_fatfs_block_offset || going_to_call_mkfs) {

        // If the FATFS is being used, then flush it to disk
        if (p->fatfp) {
            fatfs_closefs();
            fclose(p->fatfp);
            p->fatfp = NULL;

            // Check if we're doing the delayed big write of the finished FATFS product
            if (p->fatfs_buffer) {
                ssize_t rc = pwrite(fctx->output_fd, p->fatfs_buffer, p->fatfs_size, p->current_fatfs_block_offset * 512);
                free(p->fatfs_buffer);
                p->fatfs_buffer = NULL;

                if (rc != (ssize_t) p->fatfs_size)
                    ERR_RETURN("Error writing FATFS");
            }
        }

        // Handle the case where a negative block offset is used to flush
        // everything to disk, but not perform an operation.
        if (block_offset >= 0) {
            p->current_fatfs_block_offset = block_offset;

            if (going_to_call_mkfs) {
                // If we're going to call mkfs, don't bother reading anything
                p->fatfp = open_memstream(&p->fatfs_buffer, &p->fatfs_size);
                if (!p->fatfp)
                    ERR_RETURN("Error creating buffer for FATFS");

                p->fatfp_base_offset = 0;
            } else {
                // Open the destination
                int newfd = dup(fctx->output_fd);
                if (newfd < 0)
                    ERR_RETURN("Can't dup the output file handle");
                p->fatfp = fdopen(newfd, "r+");
                if (!p->fatfp)
                    ERR_RETURN("Error fdopen-ing output");

                p->fatfp_base_offset = block_offset * 512;
            }
        }
    }

    if (fatfp)
        *fatfp = p->fatfp;
    if (fatfp_offset)
        *fatfp_offset = p->fatfp_base_offset;

    return 0;
}

static int subarchive_ptr_callback(struct fun_context *fctx, const char *archive_path, struct archive **a, bool *created)
{
    struct fwup_data *p = (struct fwup_data *) fctx->cookie;

    if (created)
        *created = false;

    // Check if this is the first time to get this archive or if the path
    // has changed.
    if (!p->subarchive || strcmp(archive_path, p->subarchive_path) != 0) {
        if (p->subarchive) {
            archive_write_close(p->subarchive);
            archive_write_free(p->subarchive);
            p->subarchive = NULL;
            free(p->subarchive_path);
            p->subarchive_path = NULL;
        }

        if (archive_path) {
            p->subarchive = archive_write_new();
            archive_write_set_format_zip(p->subarchive);
            if (archive_write_open_filename(p->subarchive, archive_path) != ARCHIVE_OK) {
                archive_write_free(p->subarchive);
                p->subarchive = NULL;
                ERR_RETURN("error creating archive");
            }

            p->subarchive_path = strdup(archive_path);
            if (created)
                *created = true;
        }

    }
    if (a)
        *a = p->subarchive;
    return 0;
}

static int set_time_from_cfg(cfg_t *cfg)
{
    // The purpose of this function is to set all timestamps that we create
    // (e.g., FATFS timestamps) to the firmware creation date. This is needed
    // to make sure that the images that we create are bit-for-bit identical.
    const char *timestr = cfg_getstr(cfg, "meta-creation-date");
    if (!timestr)
        ERR_RETURN("Firmware missing meta-creation-date");

    struct tm tmp;
    if (timestamp_to_tm(timestr, &tmp) < 0)
        return -1;

    fatfs_set_time(&tmp);
    return 0;
}

int fwup_apply(const char *fw_filename, const char *task_prefix, const char *output_filename)
{
    struct fun_context fctx;
    memset(&fctx, 0, sizeof(fctx));
    fctx.output_fd = STDOUT_FILENO;
    fctx.fatfs_ptr = fatfs_ptr_callback;
    fctx.subarchive_ptr = subarchive_ptr_callback;

    struct fwup_data private_data;
    memset(&private_data, 0, sizeof(private_data));
    fctx.cookie = &private_data;

    if (output_filename) {
        fctx.output_fd = open(output_filename, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (fctx.output_fd < 0)
            ERR_RETURN("Cannot open output");
    }

    struct archive *a = archive_read_new();
    private_data.a = a;
    archive_read_support_format_zip(a);
    int rc = archive_read_open_filename(a, fw_filename, 16384);
    if (rc != ARCHIVE_OK)
        ERR_RETURN("Cannot open archive");

    struct archive_entry *ae;
    rc = archive_read_next_header(a, &ae);
    if (rc != ARCHIVE_OK)
        ERR_RETURN("Error reading archive");

    if (strcmp(archive_entry_pathname(ae), "meta.conf") != 0)
        ERR_RETURN("Expecting meta.conf to be first file");

    if (cfgfile_parse_fw_ae(a, ae, &fctx.cfg) < 0)
        return -1;

    if (set_time_from_cfg(fctx.cfg) < 0)
        return -1;

    fctx.task = find_task(fctx.cfg, task_prefix);
    if (fctx.task == 0)
        ERR_RETURN("Couldn't find applicable task");

    fctx.type = FUN_CONTEXT_INIT;
    if (run_event(&fctx, fctx.task, "on-init", NULL) < 0)
        return -1;

    fctx.type = FUN_CONTEXT_FILE;
    fctx.read = read_callback;
    while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(ae);
        if (memcmp(filename, "data/", 5) != 0)
            continue;

        const char *resource_name = &filename[5];
        if (run_event(&fctx, fctx.task, "on-resource", resource_name) < 0)
            return -1;
    }

    archive_read_free(a);

    fctx.type = FUN_CONTEXT_FINISH;
    if (run_event(&fctx, fctx.task, "on-finish", NULL) < 0)
        return -1;

    // Flush the FATFS code in case it was used.
    if (fatfs_ptr_callback(&fctx, -1, false, NULL, NULL) < 0)
        return -1;

    // Flush an subarchive that's being built.
    if (subarchive_ptr_callback(&fctx, "", NULL, NULL) < 0)
        return -1;

    close(fctx.output_fd);

    return 0;
}
