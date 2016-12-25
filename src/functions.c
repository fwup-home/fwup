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
#include "uboot_env.h"
#include "sparse_file.h"
#include "progress.h"

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
DECLARE_FUN(raw_memset);
DECLARE_FUN(fat_attrib);
DECLARE_FUN(fat_mkfs);
DECLARE_FUN(fat_write);
DECLARE_FUN(fat_mv);
DECLARE_FUN(fat_rm);
DECLARE_FUN(fat_cp);
DECLARE_FUN(fat_mkdir);
DECLARE_FUN(fat_setlabel);
DECLARE_FUN(fat_touch);
DECLARE_FUN(mbr_write);
DECLARE_FUN(uboot_clearenv);
DECLARE_FUN(uboot_setenv);
DECLARE_FUN(uboot_unsetenv);
DECLARE_FUN(error);
DECLARE_FUN(info);

struct fun_info {
    const char *name;
    int (*validate)(struct fun_context *fctx);
    int (*compute_progress)(struct fun_context *fctx);
    int (*run)(struct fun_context *fctx);
};

#define FUN_INFO(FUN) {#FUN, FUN ## _validate, FUN ## _compute_progress, FUN ## _run}
static struct fun_info fun_table[] = {
    FUN_INFO(raw_write),
    FUN_INFO(raw_memset),
    FUN_INFO(fat_attrib),
    FUN_INFO(fat_mkfs),
    FUN_INFO(fat_write),
    FUN_INFO(fat_mv),
    FUN_INFO(fat_rm),
    FUN_INFO(fat_cp),
    FUN_INFO(fat_mkdir),
    FUN_INFO(fat_setlabel),
    FUN_INFO(fat_touch),
    FUN_INFO(mbr_write),
    FUN_INFO(uboot_clearenv),
    FUN_INFO(uboot_setenv),
    FUN_INFO(uboot_unsetenv),
    FUN_INFO(error),
    FUN_INFO(info)
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

    struct sparse_file_map sfm;
    sparse_file_init(&sfm);
    OK_OR_RETURN(sparse_file_get_map_from_config(fctx->cfg, fctx->on_event->title, &sfm));
    off_t expected_length = sparse_file_data_size(&sfm);
    sparse_file_free(&sfm);

    // Count each byte as a progress unit
    fctx->progress->total_units += expected_length;

    return 0;
}
int raw_write_run(struct fun_context *fctx)
{
    assert(fctx->type == FUN_CONTEXT_FILE);
    assert(fctx->on_event);

    int rc = 0;
    struct sparse_file_map sfm;
    sparse_file_init(&sfm);

    cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", fctx->on_event->title);
    if (!resource)
        ERR_CLEANUP_MSG("raw_write can't find matching file-resource");

    char *expected_hash = cfg_getstr(resource, "blake2b-256");
    if (!expected_hash || strlen(expected_hash) != crypto_generichash_BYTES * 2)
        ERR_CLEANUP_MSG("invalid blake2b-256 hash for '%s'", fctx->on_event->title);

    OK_OR_CLEANUP(sparse_file_get_map_from_resource(resource, &sfm));
    off_t expected_length = sparse_file_data_size(&sfm);

    // Just in case we're raw writing to a FAT partition, make sure
    // that we flush any cached data.
    fctx->fatfs_ptr(fctx, -1, NULL);

    off_t dest_offset = strtoull(fctx->argv[1], NULL, 0) * 512;
    off_t len_written = 0;

    struct block_writer writer;
    OK_OR_CLEANUP(block_writer_init(&writer, fctx->output_fd, 128 * 1024, 9)); // 9 -> 512 byte blocks

    crypto_generichash_state hash_state;
    crypto_generichash_init(&hash_state, NULL, 0, crypto_generichash_BYTES);
    for (;;) {
        off_t offset;
        size_t len;
        const void *buffer;

        OK_OR_CLEANUP(fctx->read(fctx, &buffer, &len, &offset));

        // Check if done.
        if (len == 0)
            break;

        crypto_generichash_update(&hash_state, (unsigned char*) buffer, len);

        ssize_t written = block_writer_pwrite(&writer, buffer, len, dest_offset + offset);
        if (written < 0)
            ERR_CLEANUP_MSG("raw_write couldn't write %d bytes to offset %lld", len, dest_offset + offset);
        len_written += written;
        progress_report(fctx->progress, len);
    }

    off_t ending_hole = sparse_ending_hole_size(&sfm);
    if (ending_hole > 0) {
        // If this is a regular file, seeking is insufficient in making the file
        // the right length, so write a block of zeros to the end.
        char zeros[512];
        memset(zeros, 0, sizeof(zeros));
        off_t to_write = sizeof(zeros);
        if (ending_hole < to_write)
            to_write = ending_hole;
        off_t offset = sparse_file_size(&sfm) - to_write;
        ssize_t written = block_writer_pwrite(&writer, zeros, to_write, dest_offset + offset);
        if (written < 0)
            ERR_CLEANUP_MSG("raw_write couldn't write to hole at offset %lld", dest_offset + offset);

        // Unaccount for these bytes
        len_written += written - to_write;
    }

    ssize_t lastwritten = block_writer_free(&writer);
    if (lastwritten < 0)
        ERR_CLEANUP_MSG("raw_write couldn't write final bytes");
    len_written += lastwritten;

    if (len_written != expected_length) {
        if (len_written == 0)
            ERR_CLEANUP_MSG("raw_write didn't write anything. Was it called twice in an on-resource for '%s'?", fctx->on_event->title);
        else
            ERR_CLEANUP_MSG("raw_write wrote %lld bytes, but should have written %lld", len_written, expected_length);
    }

    // Verify hash
    unsigned char hash[crypto_generichash_BYTES];
    crypto_generichash_final(&hash_state, hash, sizeof(hash));
    char hash_str[sizeof(hash) * 2 + 1];
    bytes_to_hex(hash, hash_str, sizeof(hash));
    if (memcmp(hash_str, expected_hash, sizeof(hash_str)) != 0)
        ERR_CLEANUP_MSG("raw_write detected blake2b digest mismatch");

cleanup:
    sparse_file_free(&sfm);
    return rc;
}

int raw_memset_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("raw_memset requires a block offset, count, and value");

    CHECK_ARG_UINT64(fctx->argv[1], "raw_memset requires a non-negative integer block offset");
    CHECK_ARG_UINT64_MAX(fctx->argv[2], INT32_MAX / 512, "raw_memset requires a non-negative integer block count");
    CHECK_ARG_UINT64_MAX(fctx->argv[3], 255, "raw_memset requires value to be between 0 and 255");

    return 0;
}
int raw_memset_compute_progress(struct fun_context *fctx)
{
    int count = strtol(fctx->argv[2], NULL, 0);

    // Count each byte as a progress unit
    fctx->progress->total_units += count * 512;

    return 0;
}
int raw_memset_run(struct fun_context *fctx)
{
    // Just in case we're raw writing to the FAT partition, make sure
    // that we flush any cached data.
    fctx->fatfs_ptr(fctx, -1, NULL);

    const int block_size = 512;

    off_t dest_offset = strtoull(fctx->argv[1], NULL, 0) * 512;
    int count = strtol(fctx->argv[2], NULL, 0) * 512;
    int value = strtol(fctx->argv[3], NULL, 0);
    char buffer[block_size];
    memset(buffer, value, sizeof(buffer));

    struct block_writer writer;
    OK_OR_RETURN(block_writer_init(&writer, fctx->output_fd, 128 * 1024, 9)); // 9 -> 512 byte blocks

    off_t len_written = 0;
    off_t offset;
    for (offset = 0; offset < count; offset += block_size) {
        ssize_t written = block_writer_pwrite(&writer, buffer, block_size, dest_offset + offset);
        if (written < 0)
            ERR_RETURN("raw_memset couldn't write %d bytes to offset %lld", block_size, dest_offset + offset);

        len_written += written;
        progress_report(fctx->progress, written);
    }

    ssize_t lastwritten = block_writer_free(&writer);
    if (lastwritten < 0)
        ERR_RETURN("raw_memset couldn't write final bytes");
    len_written += lastwritten;
    progress_report(fctx->progress, lastwritten);

    if (len_written != count)
        ERR_RETURN("raw_memset wrote %lld bytes, but should have written %lld", len_written, count);

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
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_mkfs_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    if (fatfs_mkfs(fc, strtoul(fctx->argv[2], NULL, 0)) < 0)
        return -1;

    progress_report(fctx->progress, 1);
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
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_attrib_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    if (fatfs_attrib(fc, fctx->argv[2], fctx->argv[3]) < 0)
        return 1;

    progress_report(fctx->progress, 1);
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

    struct sparse_file_map sfm;
    sparse_file_init(&sfm);
    OK_OR_RETURN(sparse_file_get_map_from_config(fctx->cfg, fctx->on_event->title, &sfm));
    off_t expected_length = sparse_file_data_size(&sfm);
    sparse_file_free(&sfm);

    // Zero-length files still do something
    if (expected_length == 0)
        expected_length = 1;

    // Count each byte as a progress unit
    fctx->progress->total_units += expected_length;

    return 0;
}
int fat_write_run(struct fun_context *fctx)
{
    int rc = 0;
    assert(fctx->on_event);

    struct sparse_file_map sfm;
    sparse_file_init(&sfm);

    cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", fctx->on_event->title);
    if (!resource)
        ERR_CLEANUP_MSG("fat_write can't find matching file-resource");
    char *expected_hash = cfg_getstr(resource, "blake2b-256");
    if (!expected_hash || strlen(expected_hash) != crypto_generichash_BYTES * 2)
        ERR_CLEANUP_MSG("invalid blake2b-256 hash for '%s'", fctx->on_event->title);

    struct fat_cache *fc;
    off_t len_written = 0;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        ERR_CLEANUP();

    // Enforce truncation semantics if the file exists
    fatfs_rm(fc, fctx->argv[2]);

    OK_OR_CLEANUP(sparse_file_get_map_from_resource(resource, &sfm));
    off_t expected_data_length = sparse_file_data_size(&sfm);
    off_t expected_length = sparse_file_size(&sfm);

    // Handle zero-length file
    if (expected_length == 0) {
        OK_OR_CLEANUP(fatfs_touch(fc, fctx->argv[2]));

        sparse_file_free(&sfm);
        progress_report(fctx->progress, 1);
        goto cleanup;
    }

    crypto_generichash_state hash_state;
    crypto_generichash_init(&hash_state, NULL, 0, crypto_generichash_BYTES);
    for (;;) {
        off_t offset;
        size_t len;
        const void *buffer;

        OK_OR_CLEANUP(fctx->read(fctx, &buffer, &len, &offset));

        // Check if done.
        if (len == 0)
            break;

        crypto_generichash_update(&hash_state, (unsigned char*) buffer, len);

        OK_OR_CLEANUP(fatfs_pwrite(fc, fctx->argv[2], (int) offset, buffer, len));

        len_written += len;
        progress_report(fctx->progress, len);
    }

    size_t ending_hole = sparse_ending_hole_size(&sfm);
    if (ending_hole) {
        // If the file ends in a hole, fatfs_pwrite can be used to grow it.
        OK_OR_CLEANUP(fatfs_pwrite(fc, fctx->argv[2], (int) expected_length, NULL, 0));
    }

    if (len_written != expected_data_length) {
        if (len_written == 0)
            ERR_CLEANUP_MSG("fat_write didn't write anything. Was it called twice in one on-resource?");
        else
            ERR_CLEANUP_MSG("fat_write didn't write the expected amount");
    }

    unsigned char hash[crypto_generichash_BYTES];
    crypto_generichash_final(&hash_state, hash, sizeof(hash));
    char hash_str[sizeof(hash) * 2 + 1];
    bytes_to_hex(hash, hash_str, sizeof(hash));
    if (memcmp(hash_str, expected_hash, sizeof(hash_str)) != 0)
        ERR_CLEANUP_MSG("fat_write detected blake2b hash mismatch");

cleanup:
    sparse_file_free(&sfm);
    return rc;
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
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_mv_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_mv(fc, fctx->argv[2], fctx->argv[3]);

    progress_report(fctx->progress, 1);
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
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_rm_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_rm(fc, fctx->argv[2]);

    progress_report(fctx->progress, 1);
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
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_cp_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_cp(fc, fctx->argv[2], fctx->argv[3]);

    progress_report(fctx->progress, 1);
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
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_mkdir_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    // TODO: Ignore the error code here??
    fatfs_mkdir(fc, fctx->argv[2]);

    progress_report(fctx->progress, 1);
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
    fctx->progress->total_units++; // Arbitarily count as 1 unit
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
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int fat_touch_run(struct fun_context *fctx)
{
    struct fat_cache *fc;
    if (fctx->fatfs_ptr(fctx, strtoull(fctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    OK_OR_RETURN(fatfs_touch(fc, fctx->argv[2]));

    progress_report(fctx->progress, 1);
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
    fctx->progress->total_units++; // Arbitarily count as 1 unit
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
        ERR_RETURN("unexpected error writing mbr: %s", strerror(errno));

    progress_report(fctx->progress, 1);
    return 0;
}

int uboot_clearenv_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("uboot_clearenv requires a uboot-environment reference");

    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);

    if (!ubootsec)
        ERR_RETURN("uboot_clearenv can't find uboot-environment reference");

    return 0;
}
int uboot_clearenv_compute_progress(struct fun_context *fctx)
{
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int uboot_clearenv_run(struct fun_context *fctx)
{
    int rc = 0;
    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);
    struct uboot_env env;

    if (uboot_env_create_cfg(ubootsec, &env) < 0)
        return -1;

    // Just in case we're raw writing to the FAT partition, make sure
    // that we flush any cached data.
    fctx->fatfs_ptr(fctx, -1, NULL);

    char *buffer = malloc(env.env_size);
    if (uboot_env_write(&env, buffer) < 0)
        ERR_CLEANUP();

    ssize_t written = pwrite(fctx->output_fd, buffer, env.env_size, env.block_offset * 512);
    if (written != (ssize_t) env.env_size)
        ERR_CLEANUP_MSG("unexpected error writing uboot environment: %s", strerror(errno));

    progress_report(fctx->progress, 1);

cleanup:
    uboot_env_free(&env);
    free(buffer);
    return rc;
}

int uboot_setenv_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("uboot_setenv requires a uboot-environment reference, variable name and value");

    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);

    if (!ubootsec)
        ERR_RETURN("uboot_setenv can't find uboot-environment reference");

    return 0;
}
int uboot_setenv_compute_progress(struct fun_context *fctx)
{
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int uboot_setenv_run(struct fun_context *fctx)
{
    int rc = 0;
    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);
    struct uboot_env env;

    if (uboot_env_create_cfg(ubootsec, &env) < 0)
        return -1;

    // Just in case we're raw writing to the FAT partition, make sure
    // that we flush any cached data.
    fctx->fatfs_ptr(fctx, -1, NULL);

    char *buffer = malloc(env.env_size);
    ssize_t read = pread(fctx->output_fd, buffer, env.env_size, env.block_offset * 512);
    if (read != (ssize_t) env.env_size)
        ERR_CLEANUP_MSG("unexpected error reading uboot environment: %s", strerror(errno));

    if (uboot_env_read(&env, buffer) < 0)
        ERR_CLEANUP();

    if (uboot_env_setenv(&env, fctx->argv[2], fctx->argv[3]) < 0)
        ERR_CLEANUP();

    if (uboot_env_write(&env, buffer) < 0)
        ERR_CLEANUP();

    ssize_t written = pwrite(fctx->output_fd, buffer, env.env_size, env.block_offset * 512);
    if (written != (ssize_t) env.env_size)
        ERR_CLEANUP_MSG("unexpected error writing uboot environment: %s", strerror(errno));

    progress_report(fctx->progress, 1);

cleanup:
    uboot_env_free(&env);
    free(buffer);
    return rc;
}

int uboot_unsetenv_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("uboot_unsetenv requires a uboot-environment reference and a variable name");

    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);

    if (!ubootsec)
        ERR_RETURN("uboot_unsetenv can't find uboot-environment reference");

    return 0;
}
int uboot_unsetenv_compute_progress(struct fun_context *fctx)
{
    fctx->progress->total_units++; // Arbitarily count as 1 unit
    return 0;
}
int uboot_unsetenv_run(struct fun_context *fctx)
{
    int rc = 0;
    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);
    struct uboot_env env;

    if (uboot_env_create_cfg(ubootsec, &env) < 0)
        return -1;

    // Just in case we're raw writing to the FAT partition, make sure
    // that we flush any cached data.
    fctx->fatfs_ptr(fctx, -1, NULL);

    char *buffer = malloc(env.env_size);
    ssize_t read = pread(fctx->output_fd, buffer, env.env_size, env.block_offset * 512);
    if (read != (ssize_t) env.env_size)
        ERR_CLEANUP_MSG("unexpected error reading uboot environment: %s", strerror(errno));

    if (uboot_env_read(&env, buffer) < 0)
        ERR_CLEANUP();

    if (uboot_env_unsetenv(&env, fctx->argv[2]) < 0)
        ERR_CLEANUP();

    if (uboot_env_write(&env, buffer) < 0)
        ERR_CLEANUP();

    ssize_t written = pwrite(fctx->output_fd, buffer, env.env_size, env.block_offset * 512);
    if (written != (ssize_t) env.env_size)
        ERR_CLEANUP_MSG("unexpected error writing uboot environment: %s", strerror(errno));

    progress_report(fctx->progress, 1);

cleanup:
    uboot_env_free(&env);
    free(buffer);
    return rc;
}

int error_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("error() requires a message parameter");

    return 0;
}
int error_compute_progress(struct fun_context *fctx)
{
    (void) fctx; // UNUSED
    return 0;
}
int error_run(struct fun_context *fctx)
{
    ERR_RETURN("%s", fctx->argv[1]);
}

int info_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("info() requires a message parameter");

    return 0;
}
int info_compute_progress(struct fun_context *fctx)
{
    (void) fctx; // UNUSED
    return 0;
}
int info_run(struct fun_context *fctx)
{
    fwup_warnx("%s", fctx->argv[1]);
    return 0;
}
