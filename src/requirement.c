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

#include "block_cache.h"
#include "fatfs.h"
#include "mbr.h"
#include "mmc.h"
#include "uboot_env.h"
#include "util.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DECLARE_REQ(REQ) \
    static int REQ ## _validate(struct fun_context *fctx); \
    static int REQ ## _requirement_met(struct fun_context *fctx)

DECLARE_REQ(require_partition_offset);
DECLARE_REQ(require_fat_file_exists);
DECLARE_REQ(require_uboot_variable);
DECLARE_REQ(require_path_on_device);
DECLARE_REQ(require_fat_file_match);

struct req_info {
    const char *name;
    int (*validate)(struct fun_context *fctx);
    int (*requirement_met)(struct fun_context *fctx);
};

#define REQ_INFO(NAME, REQ) {NAME, REQ ## _validate, REQ ## _requirement_met}
static struct req_info req_table[] = {
    REQ_INFO("require-fat-file-exists", require_fat_file_exists),
    REQ_INFO("require-fat-file-match", require_fat_file_match),
    REQ_INFO("require-partition-offset", require_partition_offset),
    REQ_INFO("require-path-on-device", require_path_on_device),
    REQ_INFO("require-uboot-variable", require_uboot_variable)
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
 * @param fctx the function context
 * @return 0 if ok
 */
int req_validate(struct fun_context *fctx)
{
    struct req_info *req = lookup(fctx->argc, fctx->argv);
    if (!req)
        return -1;

    return req->validate(fctx);
}

/**
 * @brief Run the requirement
 *
 * This is called when applying the firmware.
 *
 * @param fctx the function context
 * @return 0 if ok
 */
int req_requirement_met(struct fun_context *fctx)
{
    struct req_info *req = lookup(fctx->argc, fctx->argv);
    if (!req)
        return -1;

    int rc = req->requirement_met(fctx);
    INFO("%s(%s%s%s) -> %s",
         req->name,
         fctx->argc >= 2 ? fctx->argv[1] : "",
         fctx->argc >= 3 ? ", " : "",
         fctx->argc >= 3 ? fctx->argv[2] : "",
         rc == 0 ? "met" : "not met");
    return rc;
}


/**
 * @brief Run all of the requirements in a reqlist
 * @param fctx the context to use (argc and argv will be updated in it)
 * @param reqlist the list
 * @param req the function to execute (currently only req_requirement_met)
 * @return 0 if ok
 */
int req_apply_reqlist(struct fun_context *fctx, cfg_opt_t *reqlist, int (*req)(struct fun_context *fctx))
{
    int ix = 0;
    char *aritystr;
    while ((aritystr = cfg_opt_getnstr(reqlist, ix++)) != NULL) {
        fctx->argc = strtoul(aritystr, NULL, 0);
        if (fctx->argc <= 0 || fctx->argc > FUN_MAX_ARGS) {
            set_last_error("Unexpected argc value in reqlist");
            return -1;
        }
        int i;
        for (i = 0; i < fctx->argc; i++) {
            fctx->argv[i] = cfg_opt_getnstr(reqlist, ix++);
            if (fctx->argv[i] == NULL) {
                set_last_error("Unexpected error with reqlist");
                return -1;
            }
        }
        // Clear out the rest of the argv entries to avoid confusion when debugging.
        for (; i < FUN_MAX_ARGS; i++)
            fctx->argv[i] = 0;

        if (req(fctx) < 0)
            return -1;
    }
    return 0;
}

int require_partition_offset_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("require-partition-offset requires a partition number and a block offset");

    int partition = strtol(fctx->argv[1], NULL, 0);
    if (partition < 0 || partition > 3)
        ERR_RETURN("require-partition-offset requires the partition number to be between 0, 1, 2, or 3");

    CHECK_ARG_UINT64(fctx->argv[2], "require-partition-offset requires a non-negative integer block offset");

    return 0;
}
int require_partition_offset_requirement_met(struct fun_context *fctx)
{
    int partition = strtol(fctx->argv[1], NULL, 0);
    off_t block_offset = strtoull(fctx->argv[2], NULL, 0);

    // Try to read the MBR. This won't work if the output
    // isn't seekable, but that's ok, since this constraint would
    // fail anyway.
    uint8_t buffer[FWUP_BLOCK_SIZE];
    if (block_cache_pread(fctx->output, buffer, FWUP_BLOCK_SIZE, 0) < 0)
        return -1;

    struct mbr_partition partitions[4];
    if (mbr_decode(buffer, partitions) < 0)
        return -1;

    if (partitions[partition].block_offset != block_offset)
        return -1;
    else
        return 0;
}

int require_fat_file_exists_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("require-fat-file-exists requires a FAT FS block offset and a filename");

    CHECK_ARG_UINT64(fctx->argv[1], "require-fat-file-exists requires a non-negative integer block offset");

    return 0;
}
int require_fat_file_exists_requirement_met(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        return -1;

    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);
    if (fatfs_exists(fctx->output, block_offset, fctx->argv[2]) < 0)
        return -1;

    // No error -> the requirement has been met.
    return 0;
}

int require_fat_file_match_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("require-fat-file-match requires a FAT FS block offset, a filename, and a pattern");

    CHECK_ARG_UINT64(fctx->argv[1], "require-fat-file-match requires a non-negative integer block offset");

    return 0;
}
int require_fat_file_match_requirement_met(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        return -1;

    off_t block_offset = strtoull(fctx->argv[1], NULL, 0);
    if (fatfs_file_matches(fctx->output, block_offset, fctx->argv[2], fctx->argv[3]) < 0)
        return -1;

    // No error -> the requirement has been met.
    return 0;
}

int require_uboot_variable_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("require-uboot-variable requires a uboot-environment reference, variable name, and value");

    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);

    if (!ubootsec)
        ERR_RETURN("require-uboot-variable can't find uboot-environment reference");

    return 0;
}
int require_uboot_variable_requirement_met(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        return -1;

    int rc = 0; // No error -> the requirement has been met.
    const char *uboot_env_name = fctx->argv[1];
    cfg_t *ubootsec = cfg_gettsec(fctx->cfg, "uboot-environment", uboot_env_name);
    struct uboot_env env;

    OK_OR_RETURN(uboot_env_create_cfg(ubootsec, &env));

    char *buffer = (char *) malloc(env.env_size);
    OK_OR_CLEANUP(block_cache_pread(fctx->output, buffer, env.env_size, env.block_offset * FWUP_BLOCK_SIZE));

    OK_OR_CLEANUP(uboot_env_read(&env, buffer));

    char *current_value;
    OK_OR_CLEANUP(uboot_env_getenv(&env, fctx->argv[2], &current_value));

    if (strcmp(current_value, fctx->argv[3]) != 0)
        rc = -1;

    free(current_value);

cleanup:
    uboot_env_free(&env);
    free(buffer);
    return rc;
}

int require_path_on_device_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("require-path-on-device requires a path and a device");

    return 0;
}
int require_path_on_device_requirement_met(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        return -1;

    const char *path = fctx->argv[1];
    const char *device = fctx->argv[2];

    if (mmc_is_path_on_device(path, device) > 0)
        return 0; // Requirement met
    else
        return -1; // Not met
}
