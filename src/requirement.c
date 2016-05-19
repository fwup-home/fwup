/*
 * Copyright 2016 Frank Hunleth
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

#include "requirement.h"
#include "util.h"
#include "mbr.h"
#include "fatfs.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DECLARE_REQ(REQ) \
    static int REQ ## _validate(struct req_context *rctx); \
    static int REQ ## _requirement_met(struct req_context *rctx)

DECLARE_REQ(require_partition_offset);
DECLARE_REQ(require_fat_file_exists);

struct req_info {
    const char *name;
    int (*validate)(struct req_context *rctx);
    int (*requirement_met)(struct req_context *rctx);
};

#define REQ_INFO(NAME, REQ) {NAME, REQ ## _validate, REQ ## _requirement_met}
static struct req_info req_table[] = {
    REQ_INFO("require-partition-offset", require_partition_offset),
    REQ_INFO("require-fat-file-exists", require_fat_file_exists)
};

static struct req_info *lookup(int argc, const char **argv)
{
    if (argc < 1) {
        set_last_error("Not enough parameters");
        return 0;
    }

    size_t i;
    for (i = 0; i < NUM_ELEMENTS(req_table); i++) {
        if (strcmp(argv[0], req_table[i].name) == 0) {
            return &req_table[i];
        }
    }

    set_last_error("Unknown function");
    return 0;
}

/**
 * @brief Validate the parameters passed to the requirement
 *
 * This is called when creating the firmware file.
 *
 * @param rctx the requirement context
 * @return 0 if ok
 */
int req_validate(struct req_context *rctx)
{
    struct req_info *req = lookup(rctx->argc, rctx->argv);
    if (!req)
        return -1;

    return req->validate(rctx);
}

/**
 * @brief Run the requirement
 *
 * This is called when applying the firmware.
 *
 * @param rctx the requirement context
 * @return 0 if ok
 */
int req_requirement_met(struct req_context *rctx)
{
    struct req_info *req = lookup(rctx->argc, rctx->argv);
    if (!req)
        return -1;

    return req->requirement_met(rctx);
}


/**
 * @brief Run all of the requirements in a reqlist
 * @param rctx the context to use (argc and argv will be updated in it)
 * @param reqlist the list
 * @param req the function to execute (currently only req_requirement_met)
 * @return 0 if ok
 */
int req_apply_reqlist(struct req_context *rctx, cfg_opt_t *reqlist, int (*req)(struct req_context *rctx))
{
    int ix = 0;
    char *aritystr;
    while ((aritystr = cfg_opt_getnstr(reqlist, ix++)) != NULL) {
        rctx->argc = strtoul(aritystr, NULL, 0);
        if (rctx->argc <= 0 || rctx->argc > REQ_MAX_ARGS) {
            set_last_error("Unexpected argc value in reqlist");
            return -1;
        }
        int i;
        for (i = 0; i < rctx->argc; i++) {
            rctx->argv[i] = cfg_opt_getnstr(reqlist, ix++);
            if (rctx->argv[i] == NULL) {
                set_last_error("Unexpected error with reqlist");
                return -1;
            }
        }
        // Clear out the rest of the argv entries to avoid confusion when debugging.
        for (; i < REQ_MAX_ARGS; i++)
            rctx->argv[i] = 0;

        if (req(rctx) < 0)
            return -1;
    }
    return 0;
}

int require_partition_offset_validate(struct req_context *rctx)
{
    if (rctx->argc != 3)
        ERR_RETURN("require-partition-offset requires a partition number and a block offset");

    int partition = strtol(rctx->argv[1], NULL, 0);
    if (partition < 0 || partition > 3)
        ERR_RETURN("require-partition-offset requires the partition number to be between 0, 1, 2, or 3");

    CHECK_ARG_UINT64(rctx->argv[2], "require-partition-offset requires a non-negative integer block offset");

    return 0;
}
int require_partition_offset_requirement_met(struct req_context *rctx)
{
    int partition = strtol(rctx->argv[1], NULL, 0);
    off_t block_offset = strtoull(rctx->argv[2], NULL, 0);

    // Try to read the MBR. This won't work if the output
    // isn't seekable, but that's ok, since this constraint would
    // fail anyway.
    uint8_t buffer[512];
    ssize_t amount_read = pread(rctx->output_fd, buffer, 512, 0);
    if (amount_read != 512)
        return -1;

    struct mbr_partition partitions[4];
    if (mbr_decode(buffer, partitions) < 0)
        return -1;

    if (partitions[partition].block_offset != block_offset)
        return -1;
    else
        return 0;
}

int require_fat_file_exists_validate(struct req_context *rctx)
{
    if (rctx->argc != 3)
        ERR_RETURN("require-fat-file-exists requires a FAT FS block offset and a filename");

    CHECK_ARG_UINT64(rctx->argv[1], "require-fat-file-exists requires a non-negative integer block offset");

    return 0;
}
int require_fat_file_exists_requirement_met(struct req_context *rctx)
{
    if (rctx->argc != 3)
        return -1;

    struct fat_cache *fc;
    if (rctx->fatfs_ptr(rctx->fctx, strtoull(rctx->argv[1], NULL, 0), &fc) < 0)
        return -1;

    if (fatfs_exists(fc, rctx->argv[2]) < 0)
        return -1;

    // No error -> the requirement has been met.
    return 0;
}

