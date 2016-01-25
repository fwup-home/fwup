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
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sodium.h>

#include "functions.h"
#include "fatfs.h"
#include "mbr.h"
#include "fwfile.h"

static bool task_is_applicable(cfg_t *task, int output_fd)
{
    int part1_offset = cfg_getint(task, "require-partition1-offset");
    if (part1_offset >= 0) {
        // Try to read the MBR. This won't work if the output
        // isn't seekable, but that's ok, since this constraint would
        // fail anyway.
        uint8_t buffer[512];
        ssize_t amount_read = pread(output_fd, buffer, 512, 0);
        if (amount_read != 512)
            return false;

        struct mbr_partition partitions[4];
        if (mbr_decode(buffer, partitions) < 0)
            return false;

        if (partitions[1].block_offset != part1_offset)
            return false;
    }

    // all constraints pass, therefore, it's ok.
    return true;
}

static cfg_t *find_task(cfg_t *cfg, int output_fd, const char *task_prefix)
{
    size_t task_len = strlen(task_prefix);
    cfg_t *task;

    int i;
    for (i = 0; (task = cfg_getnsec(cfg, "task", i)) != NULL; i++) {
        const char *name = cfg_title(task);
        if (strlen(name) >= task_len &&
                memcmp(task_prefix, name, task_len) == 0 &&
                task_is_applicable(task, output_fd))
            return task;
    }
    return 0;
}

static int apply_event(struct fun_context *fctx, cfg_t *task, const char *event_type, const char *event_parameter, int (*fun)(struct fun_context *fctx))
{
    if (event_parameter)
        fctx->on_event = cfg_gettsec(task, event_type, event_parameter);
    else
        fctx->on_event = cfg_getsec(task, event_type);

    if (fctx->on_event) {
        cfg_opt_t *funlist = cfg_getopt(fctx->on_event, "funlist");
        if (funlist) {
            if (fun_apply_funlist(fctx, funlist, fun) < 0) {
                fctx->on_event = NULL;
                return -1;
            }
        }
    }
    fctx->on_event = NULL;
    return 0;
}

struct fwup_data
{
    struct archive *a;

    bool using_fat_cache;
    struct fat_cache fc;
    off_t current_fatfs_block_offset;

    struct archive *subarchive;
    char *subarchive_path;
};

static int read_callback(struct fun_context *fctx, const void **buffer, size_t *len, off_t *offset)
{
    struct fwup_data *p = (struct fwup_data *) fctx->cookie;

    // off_t could be 32-bits so offset can't be passed directly to archive_read_data_block
    int64_t offset64 = 0;
    int rc = archive_read_data_block(p->a, buffer, len, &offset64);
    *offset = (off_t) offset64;

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

static int fatfs_ptr_callback(struct fun_context *fctx, off_t block_offset, struct fat_cache **fc)
{
    struct fwup_data *p = (struct fwup_data *) fctx->cookie;

    // Check if this is the first time or if block offset changed
    if (!p->using_fat_cache || block_offset != p->current_fatfs_block_offset) {

        // If the FATFS is being used, then flush it to disk
        if (p->using_fat_cache) {
            fatfs_closefs();
            fat_cache_free(&p->fc);
            p->using_fat_cache = false;
        }

        // Handle the case where a negative block offset is used to flush
        // everything to disk, but not perform an operation.
        if (block_offset >= 0) {
            // TODO: Make cache size configurable
            if (fat_cache_init(&p->fc, fctx->output_fd, block_offset * 512, 12 * 1024 *1024) < 0)
                return -1;

            p->using_fat_cache = true;
            p->current_fatfs_block_offset = block_offset;
        }
    }

    if (fc)
        *fc = &p->fc;

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
    OK_OR_RETURN(timestamp_to_tm(timestr, &tmp));

    fatfs_set_time(&tmp);
    return 0;
}

static void fwup_apply_report_progress(struct fun_context *fctx, int progress_units)
{
    if (fctx->progress_mode == FWUP_APPLY_NO_PROGRESS)
        return;

    fctx->current_progress_units += progress_units;
    int percent;
    if (fctx->total_progress_units > 0)
        percent = (int) ((100.0 * fctx->current_progress_units + 50.0) / fctx->total_progress_units);
    else
        percent = 0;

    // Don't report 100% until the very, very end just in case something takes
    // longer than expected in the code after all progress units have been reported.
    if (percent > 99)
        percent = 99;

    if (percent == fctx->last_progress_reported)
        return;

    fctx->last_progress_reported = percent;

    switch (fctx->progress_mode) {
    case FWUP_APPLY_NUMERIC_PROGRESS:
        printf("%d\n", percent);
        break;

    case FWUP_APPLY_NORMAL_PROGRESS:
        printf("\r%3d%%", percent);
        fflush(stdout);
        break;

    case FWUP_APPLY_NO_PROGRESS:
    default:
        break;
    }
}

static void fwup_apply_report_final_progress(struct fun_context *fctx)
{
    switch (fctx->progress_mode) {
    case FWUP_APPLY_NUMERIC_PROGRESS:
        printf("100\n");
        break;

    case FWUP_APPLY_NORMAL_PROGRESS:
        printf("\r100%%\n");
        break;

    case FWUP_APPLY_NO_PROGRESS:
    default:
        break;
    }
}

/**
 * @brief Report zero percent progress
 *
 * This function's only purpose is to improve the user experience of seeing
 * 0% progress as soon as fwup is called. See fwup.c.
 */
void fwup_apply_zero_progress(enum fwup_apply_progress progress)
{
    // Minimally initialize the fun_context so that fwup_apply_report_progress
    // works.
    struct fun_context fctx;
    memset(&fctx, 0, sizeof(fctx));
    fctx.progress_mode = progress;
    fctx.last_progress_reported = -1;
    fwup_apply_report_progress(&fctx, 0);
}

int fwup_apply(const char *fw_filename, const char *task_prefix, int output_fd, enum fwup_apply_progress progress, const unsigned char *public_key)
{
    int rc = 0;
    unsigned char *meta_conf_signature = NULL;
    struct fun_context fctx;
    memset(&fctx, 0, sizeof(fctx));
    fctx.fatfs_ptr = fatfs_ptr_callback;
    fctx.subarchive_ptr = subarchive_ptr_callback;
    fctx.progress_mode = progress;
    fctx.report_progress = fwup_apply_report_progress;
    fctx.last_progress_reported = 0; // fwup_apply_zero_progress is assumed to have been called.
    fctx.output_fd = output_fd;

    // Report 0 progress before doing anything
    fwup_apply_report_progress(&fctx, 0);

    struct fwup_data pd;
    memset(&pd, 0, sizeof(pd));
    fctx.cookie = &pd;
    pd.a = archive_read_new();

    archive_read_support_format_zip(pd.a);
    int arc = archive_read_open_filename(pd.a, fw_filename, 16384);
    if (arc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("Cannot open archive (%s): %s", fw_filename ? fw_filename : "<stdin>", archive_error_string(pd.a));

    struct archive_entry *ae;
    arc = archive_read_next_header(pd.a, &ae);
    if (arc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("Error reading archive (%s): %s", fw_filename ? fw_filename : "<stdin>", archive_error_string(pd.a));

    if (strcmp(archive_entry_pathname(ae), "meta.conf.ed25519") == 0) {
        off_t total_size;
        if (archive_read_all_data(pd.a, ae, (char **) &meta_conf_signature, crypto_sign_BYTES, &total_size) < 0)
            ERR_CLEANUP_MSG("Error reading meta.conf.ed25519 from archive.\n"
                            "Check for file corruption or libarchive built without zlib support");

        if (total_size != crypto_sign_BYTES)
            ERR_CLEANUP_MSG("Unexpected meta.conf.ed25519 size: %d", total_size);

        arc = archive_read_next_header(pd.a, &ae);
        if (arc != ARCHIVE_OK)
            ERR_CLEANUP_MSG("Expecting more than meta.conf.ed25519 in archive");
    }
    if (strcmp(archive_entry_pathname(ae), "meta.conf") != 0)
        ERR_CLEANUP_MSG("Expecting meta.conf to be at the beginning of %s", fw_filename);

    OK_OR_CLEANUP(cfgfile_parse_fw_ae(pd.a, ae, &fctx.cfg, meta_conf_signature, public_key));

    OK_OR_CLEANUP(set_time_from_cfg(fctx.cfg));

    fctx.task = find_task(fctx.cfg, fctx.output_fd, task_prefix);
    if (fctx.task == 0)
        ERR_CLEANUP_MSG("Couldn't find applicable task '%s' in %s", task_prefix, fw_filename);

    // Compute the total progress units if we're going to display it
    if (progress != FWUP_APPLY_NO_PROGRESS) {
        fctx.type = FUN_CONTEXT_INIT;
        OK_OR_CLEANUP(apply_event(&fctx, fctx.task, "on-init", NULL, fun_compute_progress));

        fctx.type = FUN_CONTEXT_FILE;

        cfg_t *sec;
        int i = 0;
        while ((sec = cfg_getnsec(fctx.task, "on-resource", i++)) != NULL) {
            cfg_t *resource = cfg_gettsec(fctx.cfg, "file-resource", sec->title);
            if (!resource) {
                // This really shouldn't happen, but failing to calculate
                // progress for a missing file-resource seems harsh.
                INFO("Can't find file-resource for %s", sec->title);
                continue;
            }

            OK_OR_CLEANUP(apply_event(&fctx, fctx.task, "on-resource", sec->title, fun_compute_progress));
        }

        fctx.type = FUN_CONTEXT_FINISH;
        OK_OR_CLEANUP(apply_event(&fctx, fctx.task, "on-finish", NULL, fun_compute_progress));
    }

    // Run
    {
        fctx.type = FUN_CONTEXT_INIT;
        OK_OR_CLEANUP(apply_event(&fctx, fctx.task, "on-init", NULL, fun_run));

        fctx.type = FUN_CONTEXT_FILE;
        fctx.read = read_callback;
        while (archive_read_next_header(pd.a, &ae) == ARCHIVE_OK) {
            const char *filename = archive_entry_pathname(ae);
            char resource_name[FWFILE_MAX_ARCHIVE_PATH];

            OK_OR_CLEANUP(archive_filename_to_resource(filename, resource_name, sizeof(resource_name)));
            OK_OR_CLEANUP(apply_event(&fctx, fctx.task, "on-resource", resource_name, fun_run));
        }

        fctx.type = FUN_CONTEXT_FINISH;
        OK_OR_CLEANUP(apply_event(&fctx, fctx.task, "on-finish", NULL, fun_run));
    }

    // Flush the FATFS code in case it was used.
    OK_OR_CLEANUP(fatfs_ptr_callback(&fctx, -1, NULL));

    // Flush a subarchive that's being built.
    OK_OR_CLEANUP(subarchive_ptr_callback(&fctx, "", NULL, NULL));

    // Close the file before we report 100% just in case that takes some time (Linux)
    close(fctx.output_fd);
    fctx.output_fd = -1;

    // Report 100% to the user
    fwup_apply_report_final_progress(&fctx);

cleanup:
    archive_read_free(pd.a);
    if (fctx.output_fd >= 0)
        close(fctx.output_fd);
    if (meta_conf_signature)
        free(meta_conf_signature);

    return rc;
}
