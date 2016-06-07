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

#include "functions.h"
#include "util.h"
#include "fatfs.h"
#include "mbr.h"
#include "fwfile.h"
#include "block_writer.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sodium.h>

#define DECLARE_FUN(FUN) \
    static int FUN ## _validate(struct fun_context *fctx); \
    static int FUN ## _compute_progress(struct fun_context *fctx); \
    static int FUN ## _run(struct fun_context *fctx)

DECLARE_FUN(raw_write);
DECLARE_FUN(fat_attrib);
DECLARE_FUN(fat_mkfs);
DECLARE_FUN(fat_write);
DECLARE_FUN(fat_mv);
DECLARE_FUN(fat_rm);
DECLARE_FUN(fat_cp);
DECLARE_FUN(fat_mkdir);
DECLARE_FUN(fat_setlabel);
DECLARE_FUN(fat_touch);
DECLARE_FUN(fw_create);
DECLARE_FUN(fw_add_local_file);
DECLARE_FUN(mbr_write);

struct fun_info {
    const char *name;
    int (*validate)(struct fun_context *fctx);
    int (*compute_progress)(struct fun_context *fctx);
    int (*run)(struct fun_context *fctx);
};

#define FUN_INFO(FUN) {#FUN, FUN ## _validate, FUN ## _compute_progress, FUN ## _run}
static struct fun_info fun_table[] = {
    FUN_INFO(raw_write),
    FUN_INFO(fat_attrib),
    FUN_INFO(fat_mkfs),
    FUN_INFO(fat_write),
    FUN_INFO(fat_mv),
    FUN_INFO(fat_rm),
    FUN_INFO(fat_cp),
    FUN_INFO(fat_mkdir),
    FUN_INFO(fat_setlabel),
    FUN_INFO(fat_touch),
    FUN_INFO(fw_create),
    FUN_INFO(fw_add_local_file),
    FUN_INFO(mbr_write),
};

static struct fun_info *lookup(int argc, const char **argv)
{
    if (argc < 1) {
        set_last_error("Not enough parameters");
        return 0;
    }

    size_t i;
    for (i = 0; i < NUM_ELEMENTS(fun_table); i++) {
        if (strcmp(argv[0], fun_table[i].name) == 0) {
            return &fun_table[i];
        }
    }

    set_last_error("Unknown function");
    return 0;
}

/**
 * @brief Validate the parameters passed to the function
 *
 * This is called when creating the firmware file.
 *
 * @param fctx the function context
 * @return 0 if ok
 */
int fun_validate(struct fun_context *fctx)
{
    struct fun_info *fun = lookup(fctx->argc, fctx->argv);
    if (!fun)
        return -1;

    return fun->validate(fctx);
}

/**
 * @brief Compute the total progress units expected
 *
 * This is called before running.
 *
 * @param fctx the function context
 * @return 0 if ok
 */
int fun_compute_progress(struct fun_context *fctx)
{
    struct fun_info *fun = lookup(fctx->argc, fctx->argv);
    if (!fun)
        return -1;

    return fun->compute_progress(fctx);
}

/**
 * @brief Run a function
 *
 * This is called when applying the firmware.
 *
 * @param fctx the function context
 * @return 0 if ok
 */
int fun_run(struct fun_context *fctx)
{
    struct fun_info *fun = lookup(fctx->argc, fctx->argv);
    if (!fun)
        return -1;

    return fun->run(fctx);
}


/**
 * @brief Run all of the functions in a funlist
 * @param fctx the context to use (argc and argv will be updated in it)
 * @param funlist the list
 * @param fun the function to execute (either fun_run or fun_compute_progress)
 * @return 0 if ok
 */
int fun_apply_funlist(struct fun_context *fctx, cfg_opt_t *funlist, int (*fun)(struct fun_context *fctx))
{
    int ix = 0;
    char *aritystr;
    while ((aritystr = cfg_opt_getnstr(funlist, ix++)) != NULL) {
        fctx->argc = strtoul(aritystr, NULL, 0);
        if (fctx->argc <= 0 || fctx->argc > FUN_MAX_ARGS) {
            set_last_error("Unexpected argc value in funlist");
            return -1;
        }
        int i;
        for (i = 0; i < fctx->argc; i++) {
            fctx->argv[i] = cfg_opt_getnstr(funlist, ix++);
            if (fctx->argv[i] == NULL) {
                set_last_error("Unexpected error with funlist");
                return -1;
            }
        }
        // Clear out the rest of the argv entries to avoid confusion when debugging.
        for (; i < FUN_MAX_ARGS; i++)
            fctx->argv[i] = 0;

        if (fun(fctx) < 0)
            return -1;
    }
    return 0;
}

int raw_write_validate(struct fun_context *fctx)
{
    if (fctx->type != FUN_CONTEXT_FILE)
        ERR_RETURN("raw_write only usable in on-resource");

    if (fctx->argc != 2)
        ERR_RETURN("raw_write requires a block offset");

    CHECK_ARG_UINT64(fctx->argv[1], "raw_write requires a non-negative integer block offset");

    return 0;
}
int raw_write_compute_progress(struct fun_context *fctx)
{
    assert(fctx->type == FUN_CONTEXT_FILE);
    assert(fctx->on_event);

    cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", fctx->on_event->title);
    if (!resource)
        ERR_RETURN("raw_write can't find matching file-resource");
    off_t expected_length = cfg_getint(resource, "length");

    // Count each byte as a progress unit
    fctx->total_progress_units += expected_length;

    return 0;
}
int raw_write_run(struct fun_context *fctx)
{
    assert(fctx->type == FUN_CONTEXT_FILE);
    assert(fctx->on_event);

    cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", fctx->on_event->title);
    if (!resource)
        ERR_RETURN("raw_write can't find matching file-resource");
    off_t expected_length = cfg_getint(resource, "length");
    char *expected_hash = cfg_getstr(resource, "blake2b-256");
    if (expected_hash && strlen(expected_hash) != crypto_generichash_BYTES * 2)
        ERR_RETURN("raw_write detected blake2b hash with the wrong length");

    // Just in case we're raw writing to the FAT partition, make sure
    // that we flush any cached data.
    fctx->fatfs_ptr(fctx, -1, NULL);

    off_t dest_offset = strtoull(fctx->argv[1], NULL, 0) * 512;
    off_t len_written = 0;

    struct block_writer writer;
    OK_OR_RETURN(block_writer_init(&writer, fctx->output_fd, 128 * 1024, 9)); // 9 -> 512 byte blocks

    crypto_generichash_state hash_state;
    crypto_generichash_init(&hash_state, NULL, 0, crypto_generichash_BYTES);
    for (;;) {
        off_t offset;
        size_t len;
        const void *buffer;

        if (fctx->read(fctx, &buffer, &len, &offset) < 0)
            return -1;

        // Check if done.
        if (len == 0)
            break;

        crypto_generichash_update(&hash_state, (unsigned char*) buffer, len);

        ssize_t written = block_writer_pwrite(&writer, buffer, len, dest_offset + offset);
        if (written < 0)
            ERR_RETURN("raw_write couldn't write %d bytes to offset %lld", len, dest_offset + offset);

        len_written += written;
        fctx->report_progress(fctx, len);
    }

    ssize_t lastwritten = block_writer_free(&writer);
    if (lastwritten < 0)
        ERR_RETURN("raw_write couldn't write final bytes");
    len_written += lastwritten;

    if (len_written != expected_length) {
        if (len_written == 0)
            ERR_RETURN("raw_write didn't write anything. Was it called twice in one on-resource?");
        else
            ERR_RETURN("raw_write wrote %lld bytes, but should have written %lld", len_written, expected_length);
    }

    // Verify hash if present.
    if (expected_hash) {
        unsigned char hash[crypto_generichash_BYTES];
        crypto_generichash_final(&hash_state, hash, sizeof(hash));
        char hash_str[sizeof(hash) * 2 + 1];
        bytes_to_hex(hash, hash_str, sizeof(hash));
        if (memcmp(hash_str, expected_hash, sizeof(hash_str)) != 0)
            ERR_RETURN("raw_write detected blake2b digest mismatch");
    }

    return 0;
}

int fat_mkfs_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_mkfs requires a block offset and block count");

    CHECK_ARG_UINT64(fctx->argv[1], "fat_mkfs requires a non-negative integer block offset");
    CHECK_ARG_UINT64(fctx->argv[2], "fat_mkfs requires a non-negative integer block count");

    return 0;
}
int fat_mkfs_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_mkfs_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    if (fatfs_mkfs(fc, strtoul(fctx->argv[2], NULL, 0)) < 0)
        return -1;

    fctx->report_progress(fctx, 1);
    return 0;
}

int fat_attrib_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("fat_attrib requires a block offset, filename, and attributes (SHR)");

    CHECK_ARG_UINT64(fctx->argv[1], "fat_mkfs requires a non-negative integer block offset");

    const char *attrib = fctx->argv[3];
    while (*attrib) {
        switch (*attrib) {
        case 'S':
        case 's':
        case 'H':
        case 'h':
        case 'R':
        case 'r':
            break;

        default:
            ERR_RETURN("fat_attrib only supports R, H, and S attributes");
        }
        attrib++;
    }
    return 0;
}
int fat_attrib_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_attrib_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    if (fatfs_attrib(fc, fctx->argv[2], fctx->argv[3]) < 0)
        return 1;

    fctx->report_progress(fctx, 1);
    return 0;
}

int fat_write_validate(struct fun_context *fctx)
{
    if (fctx->type != FUN_CONTEXT_FILE)
        ERR_RETURN("fat_write only usable in on-resource");

    if (fctx->argc != 3)
        ERR_RETURN("fat_write requires a block offset and destination filename");

    CHECK_ARG_UINT64(fctx->argv[1], "fat_write requires a non-negative integer block offset");

    return 0;
}
int fat_write_compute_progress(struct fun_context *fctx)
{
    assert(fctx->type == FUN_CONTEXT_FILE);
    assert(fctx->on_event);

    cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", fctx->on_event->title);
    if (!resource)
        ERR_RETURN("raw_write can't find matching file-resource");
    off_t expected_length = cfg_getint(resource, "length");

    // Count each byte as a progress unit
    fctx->total_progress_units += expected_length;

    return 0;
}
int fat_write_run(struct fun_context *fctx)
{
    assert(fctx->on_event);

    cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", fctx->on_event->title);
    if (!resource)
        ERR_RETURN("fat_write can't find matching file-resource");
    off_t expected_length = cfg_getint(resource, "length");
    char *expected_hash = cfg_getstr(resource, "blake2b-256");
    if (expected_hash && strlen(expected_hash) != crypto_generichash_BYTES * 2)
        ERR_RETURN("fat_write detected blake2b hash with the wrong length");

    struct fat_cache *fc;
    off_t len_written = 0;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // enforce truncation semantics if the file exists
    fatfs_rm(fc, fctx->argv[2]);

    crypto_generichash_state hash_state;
    crypto_generichash_init(&hash_state, NULL, 0, crypto_generichash_BYTES);
    for (;;) {
        off_t offset;
        size_t len;
        const void *buffer;

        if (fctx->read(fctx, &buffer, &len, &offset) < 0)
            return -1;

        // Check if done.
        if (len == 0)
            break;

        crypto_generichash_update(&hash_state, (unsigned char*) buffer, len);

        if (fatfs_pwrite(fc, fctx->argv[2], (int) offset, buffer, len) < 0)
            return -1;

        len_written += len;
        fctx->report_progress(fctx, len);
    }

    if (len_written != expected_length) {
        if (len_written == 0)
            ERR_RETURN("fat_write didn't write anything. Was it called twice in one on-resource?");
        else
            ERR_RETURN("fat_write didn't write the expected amount");
    }

    // If no hash, then skip check.
    if (expected_hash) {
        unsigned char hash[crypto_generichash_BYTES];
        crypto_generichash_final(&hash_state, hash, sizeof(hash));
        char hash_str[sizeof(hash) * 2 + 1];
        bytes_to_hex(hash, hash_str, sizeof(hash));
        if (memcmp(hash_str, expected_hash, sizeof(hash_str)) != 0)
            ERR_RETURN("fat_write detected blake2b hash mismatch");
    }

    return 0;
}

int fat_mv_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("fat_mv requires a block offset, old filename, new filename");

    CHECK_ARG_UINT64(fctx->argv[1], "fat_mv requires a non-negative integer block offset");
    return 0;
}
int fat_mv_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_mv_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_mv(fc, fctx->argv[2], fctx->argv[3]);

    fctx->report_progress(fctx, 1);
    return 0;
}

int fat_rm_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_rm requires a block offset and filename");

    CHECK_ARG_UINT64(fctx->argv[1], "fat_rm requires a non-negative integer block offset");

    return 0;
}
int fat_rm_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_rm_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_rm(fc, fctx->argv[2]);

    fctx->report_progress(fctx, 1);
    return 0;
}

int fat_cp_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("fat_cp requires a block offset, from filename, and to filename");

    CHECK_ARG_UINT64(fctx->argv[1], "fat_cp requires a non-negative integer block offset");

    return 0;
}
int fat_cp_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_cp_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_cp(fc, fctx->argv[2], fctx->argv[3]);

    fctx->report_progress(fctx, 1);
    return 0;
}

int fat_mkdir_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_mkdir requires a block offset and directory name");

    CHECK_ARG_UINT64(fctx->argv[1], "fat_mkdir requires a non-negative integer block offset");

    return 0;
}
int fat_mkdir_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_mkdir_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_mkdir(fc, fctx->argv[2]);

    fctx->report_progress(fctx, 1);
    return 0;
}

int fat_setlabel_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_setlabel requires a block offset and name");

    CHECK_ARG_UINT64(fctx->argv[1], "fat_setlabel requires a non-negative integer block offset");

    return 0;
}
int fat_setlabel_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_setlabel_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_setlabel(fc, fctx->argv[2]);

    return 0;
}

int fat_touch_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_touch requires a block offset and filename");

    CHECK_ARG_UINT64(fctx->argv[1], "fat_touch requires a non-negative integer block offset");

    return 0;
}
int fat_touch_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_touch_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    OK_OR_RETURN(fatfs_touch(fc, fctx->argv[2]));

    fctx->report_progress(fctx, 1);
    return 0;
}

int fw_create_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("fw_create requires a filename");

    return 0;
}
int fw_create_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fw_create_run(struct fun_context *fctx)
{
    struct archive *a;
    bool created;
    if (fctx->subarchive_ptr(fctx, fctx->argv[1], &a, &created) < 0)
        return -1;

    if (!created)
        ERR_RETURN("fw_create called on archive that was already open");

    if (fwfile_add_meta_conf(fctx->cfg, a, NULL) < 0)
        return -1;

    fctx->report_progress(fctx, 1);
    return 0;
}

int fw_add_local_file_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("fw_add_local_file requires a firmware filename, filename, and file with the contents");

    return 0;
}
int fw_add_local_file_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int fw_add_local_file_run(struct fun_context *fctx)
{
    struct archive *a;
    bool created;
    if (fctx->subarchive_ptr(fctx, fctx->argv[1], &a, &created) < 0)
        return -1;

    if (created)
        ERR_RETURN("call fw_create before calling fw_add_local_file");

    if (fwfile_add_local_file(a, fctx->argv[2], fctx->argv[3], NULL) < 0)
        return -1;

    fctx->report_progress(fctx, 1);
    return 0;
}

int mbr_write_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("mbr_write requires an mbr");

    const char *mbr_name = fctx->argv[1];
    cfg_t *mbrsec = cfg_gettsec(fctx->cfg, "mbr", mbr_name);

    if (!mbrsec)
        ERR_RETURN("mbr_write can't find mbr reference");

    return 0;
}
int mbr_write_compute_progress(struct fun_context *fctx)
{
    fctx->total_progress_units++; // Arbitarily count as 1 unit
    return 0;
}
int mbr_write_run(struct fun_context *fctx)
{
    const char *mbr_name = fctx->argv[1];
    cfg_t *mbrsec = cfg_gettsec(fctx->cfg, "mbr", mbr_name);
    uint8_t buffer[512];

    if (mbr_create_cfg(mbrsec, buffer) < 0)
        return -1;

    ssize_t written = pwrite(fctx->output_fd, buffer, 512, 0);
    if (written != 512)
        ERR_RETURN("unexpected error writing mbr to destination");

    fctx->report_progress(fctx, 1);
    return 0;
}
