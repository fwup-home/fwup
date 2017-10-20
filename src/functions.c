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

#include "functions.h"
#include "util.h"
#include "fatfs.h"
#include "mbr.h"
#include "fwfile.h"
#include "block_cache.h"
#include "uboot_env.h"
#include "sparse_file.h"
#include "progress.h"
#include "pad_to_block_writer.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
DECLARE_FUN(trim);
DECLARE_FUN(uboot_clearenv);
DECLARE_FUN(uboot_setenv);
DECLARE_FUN(uboot_unsetenv);
DECLARE_FUN(uboot_recover);
DECLARE_FUN(error);
DECLARE_FUN(info);
DECLARE_FUN(path_write);
DECLARE_FUN(pipe_write);
DECLARE_FUN(execute);

struct fun_info {
    const char *name;
    int (*validate)(struct fun_context *fctx);
    int (*compute_progress)(struct fun_context *fctx);
    int (*run)(struct fun_context *fctx);
};

#define FUN_INFO(FUN) {#FUN, FUN ## _validate, FUN ## _compute_progress, FUN ## _run}
#define FUN_BANG_INFO(FUN) {#FUN "!", FUN ## _validate, FUN ## _compute_progress, FUN ## _run}
static struct fun_info fun_table[] = {
    FUN_INFO(raw_write),
    FUN_INFO(raw_memset),
    FUN_INFO(fat_attrib),
    FUN_INFO(fat_mkfs),
    FUN_INFO(fat_write),
    FUN_INFO(fat_mv),
    FUN_BANG_INFO(fat_mv),
    FUN_INFO(fat_rm),
    FUN_BANG_INFO(fat_rm),
    FUN_INFO(fat_cp),
    FUN_INFO(fat_mkdir),
    FUN_INFO(fat_setlabel),
    FUN_INFO(fat_touch),
    FUN_INFO(mbr_write),
    FUN_INFO(trim),
    FUN_INFO(uboot_clearenv),
    FUN_INFO(uboot_setenv),
    FUN_INFO(uboot_unsetenv),
    FUN_INFO(uboot_recover),
    FUN_INFO(error),
    FUN_INFO(info),
    FUN_INFO(path_write),
    FUN_INFO(pipe_write),
    FUN_INFO(execute),
};

extern bool fwup_unsafe;

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

/**
 * Helper function that is paired with process_resource() to compute
 * progress.
 *
 * Set count_holes to true if holes in sparse files are manually written
 * to the destination as zeros. E.g., holes can't be optimized out by the
 * OS.
 */
static int process_resource_compute_progress(struct fun_context *fctx, bool count_holes)
{
    assert(fctx->type == FUN_CONTEXT_FILE);
    assert(fctx->on_event);

    struct sparse_file_map sfm;
    sparse_file_init(&sfm);
    OK_OR_RETURN(sparse_file_get_map_from_config(fctx->cfg, fctx->on_event->title, &sfm));
    off_t expected_length = count_holes ? sparse_file_size(&sfm) : sparse_file_data_size(&sfm);
    sparse_file_free(&sfm);

    // Count each byte as a progress unit
    fctx->progress->total_units += expected_length;

    return 0;
}
/**
 * This is a helper function for reading a resource out of a file and doing
 * something with it. (Like write it somewhere)
 *
 * It handles the following:
 *   1. Finds the resource
 *   2. Verifies that the resource contents pass the checksums
 *   3. Handles sparse reousrces
 *   4. Checks nit-picky issues and returns errors when detected
 *
 * NOTE: count_holes must match the value passed to process_resource_compute_progress.
 */
static int process_resource(struct fun_context *fctx,
                            bool count_holes,
                            int (*pwrite_callback)(void *cookie, const void *buf, size_t count, off_t offset),
                            int (*final_hole_callback)(void *cookie, off_t hole_size, off_t file_size),
                            void *cookie)
{
    assert(fctx->type == FUN_CONTEXT_FILE);
    assert(fctx->on_event);

    int rc = 0;
    struct sparse_file_map sfm;
    sparse_file_init(&sfm);

    cfg_t *resource = cfg_gettsec(fctx->cfg, "file-resource", fctx->on_event->title);
    if (!resource)
        ERR_CLEANUP_MSG("%s can't find file-resource '%s'", fctx->argv[0], fctx->on_event->title);

    char *expected_hash = cfg_getstr(resource, "blake2b-256");
    if (!expected_hash || strlen(expected_hash) != crypto_generichash_BYTES * 2)
        ERR_CLEANUP_MSG("invalid blake2b hash for '%s'", fctx->on_event->title);

    OK_OR_CLEANUP(sparse_file_get_map_from_resource(resource, &sfm));
    off_t expected_data_length = sparse_file_data_size(&sfm);

    off_t total_data_read = 0;

    crypto_generichash_state hash_state;
    crypto_generichash_init(&hash_state, NULL, 0, crypto_generichash_BYTES);

    off_t last_offset = 0;
    for (;;) {
        off_t offset;
        size_t len;
        const void *buffer;

        OK_OR_CLEANUP(fctx->read(fctx, &buffer, &len, &offset));

        // Check if done.
        if (len == 0)
            break;

        crypto_generichash_update(&hash_state, (unsigned char*) buffer, len);

        OK_OR_CLEANUP(pwrite_callback(cookie, buffer, len, offset));

        total_data_read += len;
        if (!count_holes) {
            // If not counting holes for progress reporting, then report
            // that we wrote exactly what was read.
            progress_report(fctx->progress, len);
        } else {
            // If counting holes for progress reporting, then report
            // everything since the last time.
            off_t next_offset_to_write = offset + len;
            progress_report(fctx->progress, next_offset_to_write - last_offset);
            last_offset = next_offset_to_write;
        }
    }

    // Handle a final hole in a sparse file
    off_t ending_hole = sparse_ending_hole_size(&sfm);
    if (ending_hole > 0) {
        OK_OR_CLEANUP(final_hole_callback(cookie, ending_hole, sparse_file_size(&sfm)));

        if (count_holes)
            progress_report(fctx->progress, ending_hole);
    }

    if (total_data_read != expected_data_length) {
        if (total_data_read == 0)
            ERR_CLEANUP_MSG("%s didn't write anything and was likely called twice in an on-resource for '%s'. Try a \"cp\" function.", fctx->argv[0], fctx->on_event->title);
        else
            ERR_CLEANUP_MSG("%s wrote %" PRId64" bytes for '%s', but should have written %" PRId64, fctx->argv[0], total_data_read, fctx->on_event->title, expected_data_length);
    }

    // Verify hash
    unsigned char hash[crypto_generichash_BYTES];
    crypto_generichash_final(&hash_state, hash, sizeof(hash));
    char hash_str[sizeof(hash) * 2 + 1];
    bytes_to_hex(hash, hash_str, sizeof(hash));
    if (memcmp(hash_str, expected_hash, sizeof(hash_str)) != 0)
        ERR_CLEANUP_MSG("%s detected blake2b mismatch on '%s'", fctx->argv[0], fctx->on_event->title);

cleanup:
    sparse_file_free(&sfm);
    return rc;
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
    return process_resource_compute_progress(fctx, false);
}
struct raw_write_cookie {
    off_t dest_offset;
    struct pad_to_block_writer ptbw;
};
static int raw_write_pwrite_callback(void *cookie, const void *buf, size_t count, off_t offset)
{
    struct raw_write_cookie *rwc = (struct raw_write_cookie *) cookie;
    return ptbw_pwrite(&rwc->ptbw, buf, count, rwc->dest_offset + offset);
}
static int raw_write_final_hole_callback(void *cookie, off_t hole_size, off_t file_size)
{
    struct raw_write_cookie *rwc = (struct raw_write_cookie *) cookie;

    // If this is a regular file, seeking is insufficient in making the file
    // the right length, so write a block of zeros to the end.
    char zeros[FWUP_BLOCK_SIZE];
    memset(zeros, 0, sizeof(zeros));
    off_t to_write = sizeof(zeros);
    if (hole_size < to_write)
        to_write = hole_size;
    off_t offset = file_size - to_write;
    return ptbw_pwrite(&rwc->ptbw, zeros, to_write, rwc->dest_offset + offset);
}
int raw_write_run(struct fun_context *fctx)
{
    // Raw write runs all writes through pad_to_block_writer to guarantee
    // block size writes to the caching code no matter how the input resources
    // get decompressed.

    struct raw_write_cookie rwc;
    rwc.dest_offset = strtoull(fctx->argv[1], NULL, 0) * FWUP_BLOCK_SIZE;
    ptbw_init(&rwc.ptbw, fctx->output);

    OK_OR_RETURN(process_resource(fctx,
                                  false,
                                  raw_write_pwrite_callback,
                                  raw_write_final_hole_callback,
                                  &rwc));

    return ptbw_flush(&rwc.ptbw);
}

int raw_memset_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("raw_memset requires a block offset, count, and value");

    CHECK_ARG_UINT64(fctx->argv[1], "raw_memset requires a non-negative integer block offset");
    CHECK_ARG_UINT64_RANGE(fctx->argv[2], 1, INT32_MAX / FWUP_BLOCK_SIZE, "raw_memset requires a positive integer block count");

    int value = strtol(fctx->argv[3], NULL, 0);
    if (value < 0 || value > 255)
        ERR_RETURN("raw_memset requires value to be between 0 and 255");

    return 0;
}
int raw_memset_compute_progress(struct fun_context *fctx)
{
    int count = strtol(fctx->argv[2], NULL, 0);

    // Count each byte as a progress unit
    fctx->progress->total_units += count * FWUP_BLOCK_SIZE;

    return 0;
}
int raw_memset_run(struct fun_context *fctx)
{
    const size_t block_size = FWUP_BLOCK_SIZE;

    off_t dest_offset = strtoull(fctx->argv[1], NULL, 0) * FWUP_BLOCK_SIZE;
    off_t count = strtoull(fctx->argv[2], NULL, 0) * FWUP_BLOCK_SIZE;
    int value = strtol(fctx->argv[3], NULL, 0);
    char buffer[block_size];
    memset(buffer, value, sizeof(buffer));

    off_t len_written = 0;
    off_t offset;
    for (offset = 0; offset < count; offset += block_size) {
        OK_OR_RETURN_MSG(block_cache_pwrite(fctx->output, buffer, block_size, dest_offset + offset, true),
                         "raw_memset couldn't write %d bytes to offset %" PRId64, block_size, dest_offset + offset);

        len_written += block_size;
        progress_report(fctx->progress, block_size);
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int fat_mkfs_run(struct fun_context *fctx)
{
    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);
    size_t block_count = strtoul(fctx->argv[2], NULL, 0);

    if (fatfs_mkfs(fctx->output, block_offset, block_count) < 0)
        return -1;

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int fat_attrib_run(struct fun_context *fctx)
{
    if (fatfs_attrib(fctx->output, strtoull(fctx->argv[1], NULL, 0), fctx->argv[2], fctx->argv[3]) < 0)
        return 1;

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
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
    return process_resource_compute_progress(fctx, true);
}
struct fat_write_cookie {
    struct fun_context *fctx;
    off_t block_offset;
};
static int fat_write_pwrite_callback(void *cookie, const void *buf, size_t count, off_t offset)
{
    struct fat_write_cookie *fwc = (struct fat_write_cookie *) cookie;
    struct fun_context *fctx = fwc->fctx;

    return fatfs_pwrite(fctx->output, fwc->block_offset, fctx->argv[2], (int) offset, buf, count);
}
static int fat_write_final_hole_callback(void *cookie, off_t hole_size, off_t file_size)
{
    (void) hole_size;

    struct fat_write_cookie *fwc = (struct fat_write_cookie *) cookie;
    struct fun_context *fctx = fwc->fctx;

    // If the file ends in a hole, fatfs_pwrite can be used to grow it.
    return fatfs_pwrite(fctx->output, fwc->block_offset, fctx->argv[2], (int) file_size, NULL, 0);
}
int fat_write_run(struct fun_context *fctx)
{
    struct fat_write_cookie fwc;
    fwc.fctx = fctx;
    fwc.block_offset = strtoull(fctx->argv[1], NULL, 0);

    // Enforce truncation semantics if the file exists
    OK_OR_RETURN(fatfs_truncate(fctx->output, fwc.block_offset, fctx->argv[2]));

    return process_resource(fctx,
                            true,
                            fat_write_pwrite_callback,
                            fat_write_final_hole_callback,
                            &fwc);
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int fat_mv_run(struct fun_context *fctx)
{
    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);

    bool force = (fctx->argv[0][6] == '!');
    OK_OR_RETURN(fatfs_mv(fctx->output, block_offset, fctx->argv[0], fctx->argv[2], fctx->argv[3], force));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int fat_rm_run(struct fun_context *fctx)
{
    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);

    bool file_must_exist = (fctx->argv[0][6] == '!');
    OK_OR_RETURN(fatfs_rm(fctx->output, block_offset, fctx->argv[0], fctx->argv[2], file_must_exist));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int fat_cp_run(struct fun_context *fctx)
{
    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);

    OK_OR_RETURN(fatfs_cp(fctx->output, block_offset, fctx->argv[2], fctx->argv[3]));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int fat_mkdir_run(struct fun_context *fctx)
{
    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);

    OK_OR_RETURN(fatfs_mkdir(fctx->output, block_offset, fctx->argv[2]));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int fat_setlabel_run(struct fun_context *fctx)
{
    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);

    OK_OR_RETURN(fatfs_setlabel(fctx->output, block_offset, fctx->argv[2]));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int fat_touch_run(struct fun_context *fctx)
{
    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);

    OK_OR_RETURN(fatfs_touch(fctx->output, block_offset, fctx->argv[2]));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE;
    return 0;
}
int mbr_write_run(struct fun_context *fctx)
{
    const char *mbr_name = fctx->argv[1];
    cfg_t *mbrsec = cfg_gettsec(fctx->cfg, "mbr", mbr_name);
    uint8_t buffer[FWUP_BLOCK_SIZE];

    OK_OR_RETURN(mbr_create_cfg(mbrsec, buffer));

    OK_OR_RETURN_MSG(block_cache_pwrite(fctx->output, buffer, FWUP_BLOCK_SIZE, 0, false),
                     "unexpected error writing mbr: %s", strerror(errno));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
    return 0;
}

int trim_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("trim requires a block offset and count");

    CHECK_ARG_UINT64(fctx->argv[1], "trim requires a non-negative integer block offset");
    CHECK_ARG_UINT64_RANGE(fctx->argv[2], 1, INT64_MAX / FWUP_BLOCK_SIZE, "trim requires a block count >1");

    return 0;
}
int trim_compute_progress(struct fun_context *fctx)
{
    off_t block_count = strtoull(fctx->argv[2], NULL, 0);

    // Use a heuristic for counting trim progress units -> 1 per 128KB
    fctx->progress->total_units += block_count / 256;

    return 0;
}
int trim_run(struct fun_context *fctx)
{
    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);
    off_t block_count = strtoull(fctx->argv[2], NULL, 0);

    off_t offset = block_offset * FWUP_BLOCK_SIZE;
    off_t count = block_count * FWUP_BLOCK_SIZE;

    OK_OR_RETURN(block_cache_trim(fctx->output, offset, count, true));

    progress_report(fctx->progress, block_count / 256);
    return 0;
}

int uboot_recover_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("uboot_recover requires a uboot-environment reference");

    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);

    if (!ubootsec)
        ERR_RETURN("uboot_recover can't find uboot-environment reference");

    return 0;
}
int uboot_recover_compute_progress(struct fun_context *fctx)
{
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int uboot_recover_run(struct fun_context *fctx)
{
    int rc = 0;
    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);
    struct uboot_env env;
    struct uboot_env clean_env;

    if (uboot_env_create_cfg(ubootsec, &env) < 0 ||
        uboot_env_create_cfg(ubootsec, &clean_env) < 0)
        return -1;

    char *buffer = malloc(env.env_size);
    OK_OR_CLEANUP_MSG(block_cache_pread(fctx->output, buffer, env.env_size, env.block_offset * FWUP_BLOCK_SIZE),
                      "unexpected error reading uboot environment: %s", strerror(errno));

    if (uboot_env_read(&env, buffer) < 0) {
        // Corrupt, so make a clean environment and write it.

        OK_OR_CLEANUP(uboot_env_write(&clean_env, buffer));

        OK_OR_CLEANUP_MSG(block_cache_pwrite(fctx->output, buffer, env.env_size, env.block_offset * FWUP_BLOCK_SIZE, false),
                      "unexpected error writing uboot environment: %s", strerror(errno));
    }

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);

cleanup:
    uboot_env_free(&env);
    uboot_env_free(&clean_env);
    free(buffer);
    return rc;
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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
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

    char *buffer = (char *) malloc(env.env_size);
    OK_OR_CLEANUP(uboot_env_write(&env, buffer));

    OK_OR_CLEANUP_MSG(block_cache_pwrite(fctx->output, buffer, env.env_size, env.block_offset * FWUP_BLOCK_SIZE, false),
                      "unexpected error writing uboot environment: %s", strerror(errno));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);

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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
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

    char *buffer = (char *) malloc(env.env_size);

    OK_OR_CLEANUP_MSG(block_cache_pread(fctx->output, buffer, env.env_size, env.block_offset * FWUP_BLOCK_SIZE),
                      "unexpected error reading uboot environment: %s", strerror(errno));

    OK_OR_CLEANUP(uboot_env_read(&env, buffer));

    OK_OR_CLEANUP(uboot_env_setenv(&env, fctx->argv[2], fctx->argv[3]));

    OK_OR_CLEANUP(uboot_env_write(&env, buffer));

    OK_OR_CLEANUP_MSG(block_cache_pwrite(fctx->output, buffer, env.env_size, env.block_offset * FWUP_BLOCK_SIZE, false),
                      "unexpected error writing uboot environment: %s", strerror(errno));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);

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
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
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

    char *buffer = (char *) malloc(env.env_size);
    OK_OR_CLEANUP_MSG(block_cache_pread(fctx->output, buffer, env.env_size, env.block_offset * FWUP_BLOCK_SIZE),
                      "unexpected error reading uboot environment: %s", strerror(errno));

    OK_OR_CLEANUP(uboot_env_read(&env, buffer));

    OK_OR_CLEANUP(uboot_env_unsetenv(&env, fctx->argv[2]));

    OK_OR_CLEANUP(uboot_env_write(&env, buffer));

    OK_OR_CLEANUP_MSG(block_cache_pwrite(fctx->output, buffer, env.env_size, env.block_offset * FWUP_BLOCK_SIZE, false),
                      "unexpected error writing uboot environment: %s", strerror(errno));

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);

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

static int check_unsafe(struct fun_context *fctx)
{
    if (!fwup_unsafe)
        ERR_RETURN("%s requires --unsafe", fctx->argv[0]);
    return 0;
}

int path_write_validate(struct fun_context *fctx)
{
    if (fctx->type != FUN_CONTEXT_FILE)
        ERR_RETURN("path_write only usable in on-resource");

    if (fctx->argc != 2)
        ERR_RETURN("path_write requires a file path");

    return 0;
}
int path_write_compute_progress(struct fun_context *fctx)
{
    return process_resource_compute_progress(fctx, false);
}
struct path_write_cookie {
    const char *output_filename;
    FILE *fp;
};
static int path_write_pwrite_callback(void *cookie, const void *buf, size_t count, off_t offset)
{
    struct path_write_cookie *fwc = (struct path_write_cookie *) cookie;

    OK_OR_RETURN_MSG(fseek(fwc->fp, (long) offset, SEEK_SET), "seek to offset %ld failed on '%s'", (long) offset, fwc->output_filename);

    size_t written = fwrite(buf, 1, count, fwc->fp);
    if (written != count)
        ERR_RETURN("path_write failed to write '%s'", fwc->output_filename);
    return 0;
}
static int path_write_final_hole_callback(void *cookie, off_t hole_size, off_t file_size)
{
    (void) hole_size;

    // Write a zero at the last offset to force the file to expand to the proper size
    char zero = 0;
    return path_write_pwrite_callback(cookie, &zero, 1, file_size - 1);
}
int path_write_run(struct fun_context *fctx)
{
    OK_OR_RETURN(check_unsafe(fctx));

    struct path_write_cookie pwc;
    pwc.output_filename = fctx->argv[1];
    pwc.fp = fopen(pwc.output_filename, "wb");
    if (pwc.fp == NULL)
        ERR_RETURN("path_write can't open '%s'", pwc.output_filename);

    int rc = 0;
    OK_OR_CLEANUP(process_resource(fctx,
                            false,
                            path_write_pwrite_callback,
                            path_write_final_hole_callback,
                            &pwc));

cleanup:
    fclose(pwc.fp);
    return rc;
}

int pipe_write_validate(struct fun_context *fctx)
{
    if (fctx->type != FUN_CONTEXT_FILE)
        ERR_RETURN("pipe_write only usable in on-resource");

    if (fctx->argc != 2)
        ERR_RETURN("pipe_write requires a command to execute");

    return 0;
}
int pipe_write_compute_progress(struct fun_context *fctx)
{
    return process_resource_compute_progress(fctx, true);
}
struct pipe_write_cookie {
    const char *pipe_command;
    off_t last_offset;
    FILE *fp;
};
static int pipe_write_pwrite_callback(void *cookie, const void *buf, size_t count, off_t offset)
{
    struct pipe_write_cookie *pwc = (struct pipe_write_cookie *) cookie;

    if (pwc->last_offset != offset) {
        // Fill in the gap with zeros
        char zeros[FWUP_BLOCK_SIZE];
        memset(zeros, 0, sizeof(zeros));

        while (pwc->last_offset < offset) {
            size_t to_write = offset - pwc->last_offset;
            if (to_write > sizeof(zeros))
                to_write = sizeof(zeros);
            size_t written = fwrite(zeros, 1, to_write, pwc->fp);
            if (written != count)
                ERR_RETURN("pipe_write failed to write '%s'", pwc->pipe_command);
            pwc->last_offset += to_write;
        }
    }
    size_t written = fwrite(buf, 1, count, pwc->fp);
    if (written != count)
        ERR_RETURN("pipe_write failed to write '%s'", pwc->pipe_command);
    pwc->last_offset += count;
    return 0;
}
static int pipe_write_final_hole_callback(void *cookie, off_t hole_size, off_t file_size)
{
    (void) hole_size;

    // Write a zero at the last offset to force the file to expand to the proper size
    char zero = 0;
    return pipe_write_pwrite_callback(cookie, &zero, 1, file_size - 1);
}
int pipe_write_run(struct fun_context *fctx)
{
    OK_OR_RETURN(check_unsafe(fctx));

    struct pipe_write_cookie pwc;
    pwc.pipe_command = fctx->argv[1];
    pwc.last_offset = 0;
#if defined(_WIN32) || defined(__CYGWIN__)
    pwc.fp = popen(pwc.pipe_command, "wb");
#else
    pwc.fp = popen(pwc.pipe_command, "w");
#endif
    if (!pwc.fp)
        ERR_RETURN("pipe_write can't run '%s'", pwc.pipe_command);

    int rc = 0;
    OK_OR_CLEANUP(process_resource(fctx,
                            true,
                            pipe_write_pwrite_callback,
                            pipe_write_final_hole_callback,
                            &pwc));

cleanup:
    if (pclose(pwc.fp) != 0)
        ERR_RETURN("command '%s' returned an error to pipe_write", pwc.pipe_command);

    return rc;
}

int execute_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("execute requires a command to execute");

    return 0;
}
int execute_compute_progress(struct fun_context *fctx)
{
    fctx->progress->total_units += FWUP_BLOCK_SIZE; // Arbitarily count as 1 block
    return 0;
}
int execute_run(struct fun_context *fctx)
{
    assert(fctx->on_event);
    OK_OR_RETURN(check_unsafe(fctx));

    char const *cmd_name = fctx->argv[1];
    int status = system(cmd_name);
    if (status < 0)
        ERR_RETURN("execute couldn't run '%s'", cmd_name);
    if (status != 0)
        ERR_RETURN("'%s' failed with exit status %d", cmd_name, status);

    progress_report(fctx->progress, FWUP_BLOCK_SIZE);
    return 0;
}
