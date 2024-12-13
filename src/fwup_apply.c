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
#include "monocypher.h"

#include "requirement.h"
#include "functions.h"
#include "fatfs.h"
#include "mbr.h"
#include "fwfile.h"
#include "archive_open.h"
#include "sparse_file.h"
#include "progress.h"
#include "resources.h"
#include "block_cache.h"
#include "fwup_xdelta3.h"

static bool deprecated_task_is_applicable(cfg_t *task, struct block_cache *output)
{
    // Handle legacy require-partition1-offset=x constraint
    int part1_offset = cfg_getint(task, "require-partition1-offset");
    if (part1_offset >= 0) {
        // Try to read the MBR. This won't work if the output
        // isn't seekable, but that's ok, since this constraint would
        // fail anyway.
        uint8_t buffer[FWUP_BLOCK_SIZE];
        if (block_cache_pread(output, buffer, FWUP_BLOCK_SIZE, 0) < 0)
            return false;

        struct mbr_table table;
        if (mbr_decode(buffer, &table) < 0)
            return false;

        if (table.partitions[1].block_offset != (uint32_t) part1_offset)
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
                deprecated_task_is_applicable(task, fctx->output) &&
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
};

static int read_callback_normal(struct fun_context *fctx, const void **buffer, size_t *len, off_t *offset)
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
    int64_t ignored;
    int rc = fwup_archive_read_data_block(p->a, buffer, len, &ignored);

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

static int xdelta_read_patch_callback(void* cookie, const void **buffer, size_t *len)
{
    struct fun_context *fctx = (struct fun_context *) cookie;
    struct fwup_apply_data *p = (struct fwup_apply_data *) fctx->cookie;

    int64_t ignored;
    int rc = fwup_archive_read_data_block(p->a, buffer, len, &ignored);

    if (rc == ARCHIVE_OK) {
        return 0;
    } else if (rc == ARCHIVE_EOF) {
        *len = 0;
        *buffer = NULL;
        return 0;
    } else {
        *len = 0;
        *buffer = NULL;
        ERR_RETURN(archive_error_string(p->a));
    }
}

static int xdelta_read_source_callback(void *cookie, void *buf, size_t count, off_t offset)
{
    struct fun_context *fctx = (struct fun_context *) cookie;

    if (offset < 0 ||
        offset > fctx->xd_source_count)
        ERR_RETURN("xdelta tried to load outside of allowed byte range (0-%" PRId64 "): offset: %" PRId64 ", count: %d", fctx->xd_source_count, offset, count);

    if (count > fctx->xd_source_count ||
        offset > fctx->xd_source_count - count)
        count = fctx->xd_source_count - offset;

    return block_cache_pread(fctx->output, buf, count, fctx->xd_source_offset + offset);
}

static int xdelta_read_fat_callback(void *cookie, void *buf, size_t count, off_t offset)
{
    struct fun_context *fctx = (struct fun_context *) cookie;

    // fatfs_pread checks bounds so it will catch xdelta3 errors
    return fatfs_pread(fctx->output, fctx->xd_source_offset / FWUP_BLOCK_SIZE, fctx->xd_source_path, offset, count, buf);
}

static int read_callback_xdelta(struct fun_context *fctx, const void **buffer, size_t *len, off_t *offset)
{
    struct fwup_apply_data *p = (struct fwup_apply_data *) fctx->cookie;

    // This would be ridiculous...
    if (p->sfm.map_len != 1)
        ERR_RETURN("Sparse xdelta not supported");

    OK_OR_RETURN(xdelta_read(fctx->xd, buffer, len));

    // xdelta_read's output has no holes
    *offset = p->actual_offset;
    p->actual_offset += *len;

    return 0;
}

static int read_callback(struct fun_context *fctx, const void **buffer, size_t *len, off_t *offset)
{
    if (fctx->xd)
        return read_callback_xdelta(fctx, buffer, len, offset);
    else
        return read_callback_normal(fctx, buffer, len, offset);
}

static void initialize_timestamps()
{
    // The purpose of this function is to set all timestamps that we create
    // (e.g., FATFS timestamps) to a fixed date. This is needed
    // to make sure that the images that we create are bit-for-bit identical.

    // Set the timestamp to FAT time 0
    struct tm tmp;
    tmp.tm_year = 80;
    tmp.tm_mon = 0;
    tmp.tm_mday = 1;
    tmp.tm_hour = 0;
    tmp.tm_min = 0;
    tmp.tm_sec = 0;

    fatfs_set_time(&tmp);
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
                // but it doesn't have another data block. This means that it's
                // either a 0-length file or it's all sparse. Signal EOF. This
                // might be a bug, but I can't think of a real use case for a completely
                // sparse file.
                pd->sparse_map_ix = pd->sfm.map_len;
            }
        }

// MOVE ME!!!
{
    cfg_t *on_resource = cfg_gettsec(fctx->task, "on-resource", resource_name);
    if (on_resource) {
        off_t size_in_archive = archive_entry_size(ae);
        off_t expected_size_in_archive = sparse_file_data_size(&pd->sfm);

        if (pd->sfm.map_len == 1 && archive_entry_size_is_set(ae) && size_in_archive != expected_size_in_archive) {
            // Size in archive is different from expected size
            const char *source_raw_offset_str = cfg_getstr(on_resource, "delta-source-raw-offset");
            int source_raw_count = cfg_getint(on_resource, "delta-source-raw-count");

            const char *source_fat_offset_str = cfg_getstr(on_resource, "delta-source-fat-offset");
            const char *source_fat_path = cfg_getstr(on_resource, "delta-source-fat-path");

            if (source_raw_count > 0 && source_raw_offset_str != NULL) {
                // Found delta-source-raw-offset and delta-source-raw-count directives
                off_t source_raw_offset = strtoul(source_raw_offset_str, NULL, 0);

                fctx->xd = malloc(sizeof(struct xdelta_state));
                xdelta_init(fctx->xd, xdelta_read_patch_callback, xdelta_read_source_callback, fctx);
                fctx->xd_source_offset = source_raw_offset * FWUP_BLOCK_SIZE;
                fctx->xd_source_count = source_raw_count * FWUP_BLOCK_SIZE;
                fctx->xd_source_path = NULL;
            } else if (source_fat_offset_str != NULL && source_fat_path != NULL) {
                // Found delta-source-fat-offset and delta-source-fat-path directives
                off_t source_fat_offset = strtoul(source_fat_offset_str, NULL, 0);

                fctx->xd = malloc(sizeof(struct xdelta_state));
                xdelta_init(fctx->xd, xdelta_read_patch_callback, xdelta_read_fat_callback, fctx);
                fctx->xd_source_offset = source_fat_offset * FWUP_BLOCK_SIZE;
                fctx->xd_source_path = source_fat_path;
                fctx->xd_source_count = 0; // unused
            } else {
                ERR_CLEANUP_MSG("File '%s' isn't expected size (%d vs %d) and xdelta3 patch support not enabled on it. (Add delta-source-raw-offset / delta-source-raw-count or delta-source-fat-offset / delta-source-fat-path)", resource_name, (int) size_in_archive, (int) expected_size_in_archive);
            }
        }
    }
}
        OK_OR_CLEANUP(apply_event(fctx, fctx->task, "on-resource", resource_name, fun_run));

        item->processed = true;
        sparse_file_free(&pd->sfm);

{ // MOVE ME!!!
    if (fctx->xd) {
        xdelta_free(fctx->xd);
        free(fctx->xd);
        fctx->xd = NULL;
    }
}
    }

    // Make sure that all "on-resource" blocks have been run.
    for (const struct resource_list *r = resources; r != NULL; r = r->next) {
        if (!r->processed)
            ERR_CLEANUP_MSG("Resource %s not found in archive", cfg_title(r->resource));
    }

    // Flush any fatfs filesystem updates before the on-finish. The on-finish
    // generally has A/B partition swaps or other critical operations that
    // assume the prior data has already been written. The block_cache will
    // write output in the right order, but fatfs's caching will not so it
    // needs to be flushed here.
    fatfs_closefs();

    fctx->type = FUN_CONTEXT_FINISH;
    OK_OR_CLEANUP(apply_event(fctx, fctx->task, "on-finish", NULL, fun_run));

cleanup:
    if (rc != 0) {
        // Do a best attempt at running any error handling code
        fctx->type = FUN_CONTEXT_ERROR;

        // Since there's an error, throw out anything that's in the cache so
        // that the error handler can start fresh.
        fatfs_closefs();
        block_cache_reset(fctx->output);

        if (apply_event(fctx, fctx->task, "on-error", NULL, fun_run) < 0) {
            // Yet another error so throw out the cache again.
            fatfs_closefs();
            block_cache_reset(fctx->output);
        }
    }
    rlist_free(resources);
    return rc;
}

int fwup_apply(const char *fw_filename,
               const char *task_prefix,
               int output_fd,
               off_t end_offset,
               struct fwup_progress *progress,
               unsigned char *const*public_keys,
               bool enable_trim,
               bool verify_writes,
               bool minimize_writes)
{
    int rc = 0;
    unsigned char *meta_conf_signature = NULL;
    struct fun_context fctx;
    memset(&fctx, 0, sizeof(fctx));
    fctx.progress = progress;

    // Report 0 progress before doing anything
    progress_report(fctx.progress, 0);

    struct fwup_apply_data pd;
    memset(&pd, 0, sizeof(pd));
    fctx.cookie = &pd;
    pd.a = archive_read_new();

    archive_read_support_format_zip(pd.a);
    int arc = fwup_archive_open_filename(pd.a, fw_filename, progress);
    if (arc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("%s", archive_error_string(pd.a));

    struct archive_entry *ae;
    arc = archive_read_next_header(pd.a, &ae);
    if (arc != ARCHIVE_OK)
        ERR_CLEANUP_MSG("%s", archive_error_string(pd.a));

    if (strcmp(archive_entry_pathname(ae), "meta.conf.ed25519") == 0) {
        off_t total_size;
        if (archive_read_all_data(pd.a, ae, (char **) &meta_conf_signature, FWUP_SIGNATURE_LEN, &total_size) < 0)
            ERR_CLEANUP_MSG("Error reading meta.conf.ed25519 from archive.\n"
                            "Check for file corruption or libarchive built without zlib support");

        if (total_size != FWUP_SIGNATURE_LEN)
            ERR_CLEANUP_MSG("Unexpected meta.conf.ed25519 size: %d", total_size);

        arc = archive_read_next_header(pd.a, &ae);
        if (arc != ARCHIVE_OK)
            ERR_CLEANUP_MSG("Expecting more than meta.conf.ed25519 in archive");
    }
    if (strcmp(archive_entry_pathname(ae), "meta.conf") != 0)
        ERR_CLEANUP_MSG("Expecting meta.conf to be at the beginning of %s", fw_filename);

    OK_OR_CLEANUP(cfgfile_parse_fw_ae(pd.a, ae, &fctx.cfg, meta_conf_signature, public_keys));

    initialize_timestamps();

    // Initialize the output. Nothing should have been written before now
    // and waiting to initialize the output until now forces the point.
    fctx.output = (struct block_cache *) malloc(sizeof(struct block_cache));
    OK_OR_CLEANUP(block_cache_init(fctx.output, output_fd, end_offset, enable_trim, verify_writes, minimize_writes));

    // Go through all of the tasks and find a matcher
    fctx.task = find_task(&fctx, task_prefix);
    if (fctx.task == 0)
        ERR_CLEANUP_MSG("Couldn't find applicable task '%s'. If task is available, the task's requirements may not be met.", task_prefix);

    // Compute the total progress units
    OK_OR_CLEANUP(compute_progress(&fctx));

    // Run
    OK_OR_CLEANUP(run_task(&fctx, &pd));

    // Flush everything
    fatfs_closefs();
    OK_OR_CLEANUP(block_cache_flush(fctx.output));

    // Close everything before reporting 100% just in case the OS blocks on the close call.
    block_cache_free(fctx.output);
    free(fctx.output);
    fctx.output = NULL;
    close(output_fd);

    // Success -> report 100%
    progress_report_complete(fctx.progress);

cleanup:
    // Close the output
    if (fctx.output) {
        // In the error case, flush in case the on-error
        // handler left something. Errors in on-error are
        // handled with the on-error call.
        fatfs_closefs();
        block_cache_flush(fctx.output); // Ignore errors

        block_cache_free(fctx.output);
        free(fctx.output);
        fctx.output = NULL;
        close(output_fd);
    }

    sparse_file_free(&pd.sfm);

    archive_read_free(pd.a);

    if (meta_conf_signature)
        free(meta_conf_signature);

    if (fctx.cfg) {
        cfg_free(fctx.cfg);
        fctx.cfg = NULL;
    }

    return rc;
}
