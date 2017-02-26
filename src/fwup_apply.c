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

#include "requirement.h"
#include "functions.h"
#include "fatfs.h"
#include "mbr.h"
#include "fwfile.h"
#include "archive_open.h"
#include "sparse_file.h"
#include "progress.h"
#include "resources.h"

static bool deprecated_task_is_applicable(cfg_t *task, int output_fd)
{
    // Handle legacy require-partition1-offset=x constraint
    int part1_offset = cfg_getint(task, "require-partition1-offset");
    if (part1_offset >= 0) {
        // Try to read the MBR. This won't work if the output
        // isn't seekable, but that's ok, since this constraint would
        // fail anyway.
        uint8_t buffer[512];
        ssize_t amount_read = aligned_pread(output_fd, buffer, 512, 0);
        if (amount_read != 512)
            return false;

        struct mbr_partition partitions[4];
        if (mbr_decode(buffer, partitions) < 0)
            return false;

        if (partitions[1].block_offset != (uint32_t) part1_offset)
            return false;
    }

    // all constraints pass, therefore, it's ok.
    return true;
}

static bool task_is_applicable(struct fun_context *fctx, cfg_t *task)
{
    cfg_opt_t *reqlist = cfg_getopt(task, "reqlist");
    if (reqlist) {
        if (req_apply_reqlist(fctx, reqlist, req_requirement_met) < 0) {
            // Error indicates that one or more requirements weren't met or
            // something was messed up in the requirement. Either way, the
            // task isn't applicable.
            return false;
        }
    }

    // If we get here, then it's ok to apply this task.
    return true;
}

static cfg_t *find_task(struct fun_context *fctx, const char *task_prefix)
{
    size_t task_len = strlen(task_prefix);
    cfg_t *task;

    int i;
    for (i = 0; (task = cfg_getnsec(fctx->cfg, "task", i)) != NULL; i++) {
        const char *name = cfg_title(task);
        if (strlen(name) >= task_len &&
                memcmp(task_prefix, name, task_len) == 0 &&
                deprecated_task_is_applicable(task, fctx->output_fd) &&
                task_is_applicable(fctx, task))
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

struct fwup_apply_data
{
    struct archive *a;
    bool reading_stdin;

    // Sparse file handling
    struct sparse_file_map sfm;
    int sparse_map_ix;
    off_t sparse_block_offset;
    off_t actual_offset;
    const void *sparse_leftover;
    off_t sparse_leftover_len;

    // FAT file system support
    bool using_fat_cache;
    struct fat_cache fc;
    off_t current_fatfs_block_offset;
};

static int read_callback(struct fun_context *fctx, const void **buffer, size_t *len, off_t *offset)
{
    struct fwup_apply_data *p = (struct fwup_apply_data *) fctx->cookie;

    // Even though libarchive's API supports sparse files, the ZIP file format
    // does not support them, so it can't be used. To workaround this, all of the data
    // chunks of a sparse file are concatenated together. This function breaks them
    // apart.

    if (p->sparse_map_ix == p->sfm.map_len) {
        // End of file
        *len = 0;
        *buffer = NULL;
        *offset = 0;
        return 0;
    }
    off_t sparse_file_chunk_len = p->sfm.map[p->sparse_map_ix];
    off_t remaining_data_in_sparse_file_chunk =
            sparse_file_chunk_len - p->sparse_block_offset;

    if (p->sparse_leftover_len > 0) {
        // Handle the case where a previous call had data remaining
        *buffer = p->sparse_leftover;
        *offset = p->actual_offset;
        if (remaining_data_in_sparse_file_chunk >= p->sparse_leftover_len)
            *len = p->sparse_leftover_len;
        else
            *len = remaining_data_in_sparse_file_chunk;

        p->sparse_leftover += *len;
        p->sparse_leftover_len -= *len;
        p->actual_offset += *len;
        p->sparse_block_offset += *len;
        if (p->sparse_block_offset == sparse_file_chunk_len) {
            // Advance over hole (unless this is the end)
            p->sparse_map_ix++;
            p->sparse_block_offset = 0;
            if (p->sparse_map_ix != p->sfm.map_len) {
                p->actual_offset += p->sfm.map[p->sparse_map_ix];

                // Advance to next data block
                p->sparse_map_ix++;
            }
        }
        return 0;
    }

    // Decompress more data

    // off_t could be 32-bits so offset can't be passed directly to archive_read_data_block
    int64_t offset64 = 0;
    int rc = archive_read_data_block(p->a, buffer, len, &offset64);

    // Handle case where archive_read_data_block returns a 0 byte read
    // even though it's not at end of file. A second read gets past this.
    if (rc == ARCHIVE_OK && *len == 0)
        rc = archive_read_data_block(p->a, buffer, len, &offset64);

    if (rc == ARCHIVE_EOF) {
        *len = 0;
        *buffer = NULL;
        *offset = 0;
        return 0;
    } else if (rc != ARCHIVE_OK)
        ERR_RETURN(archive_error_string(p->a));

    *offset = p->actual_offset;

    if (remaining_data_in_sparse_file_chunk > (off_t) *len) {
        // The amount decompressed doesn't cross a sparse file hole
        p->actual_offset += *len;
        p->sparse_block_offset += *len;
    } else {
        // The amount decompressed crosses a hole in a sparse file,
        // so return the contiguous chunk and save the leftovers.
        p->actual_offset += remaining_data_in_sparse_file_chunk;
        p->sparse_leftover_len = *len - remaining_data_in_sparse_file_chunk;
        p->sparse_leftover = *buffer + remaining_data_in_sparse_file_chunk;

        *len = remaining_data_in_sparse_file_chunk;

        // Advance over hole (unless this is the end)
        p->sparse_map_ix++;
        p->sparse_block_offset = 0;
        if (p->sparse_map_ix != p->sfm.map_len) {
            p->actual_offset += p->sfm.map[p->sparse_map_ix];

            // Advance to next data block
            p->sparse_map_ix++;
        }
    }

    return 0;
}

static int fatfs_ptr_callback(struct fun_context *fctx, off_t block_offset, struct fat_cache **fc)
{
    struct fwup_apply_data *p = (struct fwup_apply_data *) fctx->cookie;

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

static int set_time_from_cfg(cfg_t *cfg)
{
    // The purpose of this function is to set all timestamps that we create
    // (e.g., FATFS timestamps) to the firmware creation date. This is needed
    // to make sure that the images that we create are bit-for-bit identical.
    const char *timestr = cfg_getstr(cfg, "meta-creation-date");

    struct tm tmp;
    if (timestr) {
        // Set the timestamp to the creation time
        OK_OR_RETURN(timestamp_to_tm(timestr, &tmp));
    } else {
        // Set the timestamp to FAT time 0
        tmp.tm_year = 80;
        tmp.tm_mon = 0;
        tmp.tm_mday = 0;
        tmp.tm_hour = 0;
        tmp.tm_min = 0;
        tmp.tm_sec = 0;
    }

    fatfs_set_time(&tmp);
    return 0;
}

static int compute_progress(struct fun_context *fctx)
{
    fctx->type = FUN_CONTEXT_INIT;
    OK_OR_RETURN(apply_event(fctx, fctx->task, "on-init", NULL, fun_compute_progress));

    fctx->type = FUN_CONTEXT_FILE;
    cfg_t *sec;
    int i = 0;
    while ((sec = cfg_getnsec(fctx->task, "on-resource", i++)) != NULL) {
        cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", sec->title);
        if (!resource) {
            // This really shouldn't happen, but failing to calculate
            // progress for a missing file-resource seems harsh.
            INFO("Can't find file-resource for %s", sec->title);
            continue;
        }

        OK_OR_RETURN(apply_event(fctx, fctx->task, "on-resource", sec->title, fun_compute_progress));
    }

    fctx->type = FUN_CONTEXT_FINISH;
    OK_OR_RETURN(apply_event(fctx, fctx->task, "on-finish", NULL, fun_compute_progress));

    return 0;
}

static int run_task(struct fun_context *fctx, struct fwup_apply_data *pd)
{
    int rc = 0;

    struct resource_list *resources = NULL;
    OK_OR_CLEANUP(rlist_get_from_task(fctx->cfg, fctx->task, &resources));

    fctx->type = FUN_CONTEXT_INIT;
    OK_OR_CLEANUP(apply_event(fctx, fctx->task, "on-init", NULL, fun_run));

    fctx->type = FUN_CONTEXT_FILE;
    fctx->read = read_callback;
    struct archive_entry *ae;
    while (archive_read_next_header(pd->a, &ae) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(ae);
        char resource_name[FWFILE_MAX_ARCHIVE_PATH];

        OK_OR_CLEANUP(archive_filename_to_resource(filename, resource_name, sizeof(resource_name)));

        // Skip an empty filename. This is easy to get when you run 'zip'
        // on the command line to create a firmware update file and include
        // the 'data' directory. It's annoying when it creates an error
        // (usually when debugging something else), so ignore it.
        if (resource_name[0] == '\0')
            continue;

        // See if this resource is used by this task
        struct resource_list *item = rlist_find_by_name(resources, resource_name);
        if (item == NULL)
            continue;

        // See if there's metadata associated with this resource
        if (item->resource == NULL)
            ERR_CLEANUP_MSG("Resource '%s' used, but metadata is missing. Archive is corrupt.", resource_name);

        OK_OR_CLEANUP(sparse_file_get_map_from_resource(item->resource, &pd->sfm));
        pd->sparse_map_ix = 0;
        pd->sparse_block_offset = 0;
        pd->actual_offset = 0;
        pd->sparse_leftover = NULL;
        pd->sparse_leftover_len = 0;
        if (pd->sfm.map[0] == 0) {
            if (pd->sfm.map_len > 2) {
                // This is the case where there's a hole at the beginning. Advance to
                // the offset of the data.
                pd->sparse_map_ix = 2;
                pd->actual_offset = pd->sfm.map[1];
            } else {
                // sparse map has a 0 length data block and possibly a hole,
                // but it doesn't have anoter data block. This means that it's
                // either a 0-length file or it's all sparse. Signal EOF. This
                // might be a bug, but I can't think of a real use case for a completely
                // sparse file.
                pd->sparse_map_ix = pd->sfm.map_len;
            }
        }

        OK_OR_CLEANUP(apply_event(fctx, fctx->task, "on-resource", resource_name, fun_run));

        item->processed = true;
        sparse_file_free(&pd->sfm);
    }

    // Make sure that all "on-resource" blocks have been run.
    for (const struct resource_list *r = resources; r != NULL; r = r->next) {
        if (!r->processed)
            ERR_CLEANUP_MSG("Resource %s not found in archive", cfg_title(r->resource));
    }

    fctx->type = FUN_CONTEXT_FINISH;
    OK_OR_CLEANUP(apply_event(fctx, fctx->task, "on-finish", NULL, fun_run));

    // Flush the FATFS code in case it was used.
    OK_OR_CLEANUP(fatfs_ptr_callback(fctx, -1, NULL));

cleanup:
    if (rc != 0) {
        // Do a best attempt at running any error handling code
        fctx->type = FUN_CONTEXT_ERROR;
        if (apply_event(fctx, fctx->task, "on-error", NULL, fun_run) == 0) {
            // If the error handler is successful, then we may need
            // to flush the FATFS code again.
            fatfs_ptr_callback(fctx, -1, NULL);
        }
    }
    rlist_free(resources);
    return rc;
}

int fwup_apply(const char *fw_filename,
               const char *task_prefix,
               int output_fd,
               struct fwup_progress *progress,
               const unsigned char *public_key)
{
    int rc = 0;
    unsigned char *meta_conf_signature = NULL;
    struct fun_context fctx;
    memset(&fctx, 0, sizeof(fctx));
    fctx.fatfs_ptr = fatfs_ptr_callback;
    fctx.progress = progress;
    fctx.output_fd = output_fd;

    // Report 0 progress before doing anything
    progress_report(fctx.progress, 0);

    struct fwup_apply_data pd;
    memset(&pd, 0, sizeof(pd));
    fctx.cookie = &pd;
    pd.a = archive_read_new();

    bool reading_stdin = (fw_filename == NULL || fw_filename[0] == '\0');

    archive_read_support_format_zip(pd.a);
    int arc = fwup_archive_open_filename(pd.a, fw_filename);
    if (arc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("%s", archive_error_string(pd.a));

    struct archive_entry *ae;
    arc = archive_read_next_header(pd.a, &ae);
    if (arc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("%s", archive_error_string(pd.a));

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

    fctx.task = find_task(&fctx, task_prefix);
    if (fctx.task == 0)
        ERR_CLEANUP_MSG("Couldn't find applicable task '%s' in %s. If task is available, the task's requirements may not be met.", task_prefix, fw_filename);

    // Compute the total progress units
    OK_OR_CLEANUP(compute_progress(&fctx));

    // Run
    OK_OR_CLEANUP(run_task(&fctx, &pd));

cleanup:
    // Close the file before we report 100% just in case that takes some time (Linux)
    close(fctx.output_fd);
    fctx.output_fd = -1;

    sparse_file_free(&pd.sfm);

    // Report 100% to the user if successful
    if (rc == 0)
        progress_report_complete(fctx.progress);

    if (fctx.output_fd >= 0)
        close(fctx.output_fd);

    // If reading stdin, signal that we're not going to read any more
    // so that pipes can be terminated, etc. libarchive not only doesn't
    // do this for us, it also reads the rest of the input in archive_read_free
    // which if there's a lot left, it will take awhile.
    if (reading_stdin)
        close(STDIN_FILENO);

    archive_read_free(pd.a);

    if (meta_conf_signature)
        free(meta_conf_signature);

    return rc;
}
