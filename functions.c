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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int raw_write_validate(struct fun_context *fctx);
static int raw_write_run(struct fun_context *fctx);
static int fat_mkfs_validate(struct fun_context *fctx);
static int fat_mkfs_run(struct fun_context *fctx);
static int fat_write_validate(struct fun_context *fctx);
static int fat_write_run(struct fun_context *fctx);
static int fat_mv_validate(struct fun_context *fctx);
static int fat_mv_run(struct fun_context *fctx);
static int fat_rm_validate(struct fun_context *fctx);
static int fat_rm_run(struct fun_context *fctx);
static int fw_create_validate(struct fun_context *fctx);
static int fw_create_run(struct fun_context *fctx);
static int fw_add_local_file_validate(struct fun_context *fctx);
static int fw_add_local_file_run(struct fun_context *fctx);
static int mbr_write_validate(struct fun_context *fctx);
static int mbr_write_run(struct fun_context *fctx);

struct fun_info {
    const char *name;
    int (*validate)(struct fun_context *fctx);
    int (*run)(struct fun_context *fctx);
};

static struct fun_info fun_table[] = {
    {"raw_write", raw_write_validate, raw_write_run },
    {"fat_mkfs", fat_mkfs_validate, fat_mkfs_run },
    {"fat_write", fat_write_validate, fat_write_run },
    {"fat_mv", fat_mv_validate, fat_mv_run },
    {"fat_rm", fat_rm_validate, fat_rm_run },
    {"fw_create", fw_create_validate, fw_create_run },
    {"fw_add_local_file", fw_add_local_file_validate, fw_add_local_file_run },
    {"mbr_write", mbr_write_validate, mbr_write_run }
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
 * @brief Run a function
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
 * @return 0 if ok
 */
int fun_run_funlist(struct fun_context *fctx, cfg_opt_t *funlist)
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

        if (fun_run(fctx) < 0)
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

    int offset = strtoul(fctx->argv[1], 0, 0);
    if (offset < 0)
        ERR_RETURN("block offset should be non-negative");

    return 0;
}

int raw_write_run(struct fun_context *fctx)
{
    assert(fctx->type == FUN_CONTEXT_FILE);

    int dest_offset = strtoul(fctx->argv[1], NULL, 0) * 512;

    for (;;) {
        int64_t offset;
        size_t len;
        const void *buffer;

        if (fctx->read(fctx, &buffer, &len, &offset) < 0)
            return -1;

        // Check if done.
        if (len == 0)
            break;

        ssize_t written = pwrite(fctx->output_fd, buffer, len, dest_offset + offset);
        if (written != (ssize_t) len)
            ERR_RETURN("unexpected error writing to destination");
    }

    return 0;
}

int fat_mkfs_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_mkfs requires a block offset and block count");

    return 0;
}

int fat_mkfs_run(struct fun_context *fctx)
{
    return 0;
}

int fat_write_validate(struct fun_context *fctx)
{
    if (fctx->type != FUN_CONTEXT_FILE)
        ERR_RETURN("fat_write only usable in on-resource");

    if (fctx->argc != 3)
        ERR_RETURN("fat_write requires a block offset and destination filename");

    return 0;
}
int fat_write_run(struct fun_context *fctx)
{
    return 0;
}

int fat_mv_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("fat_mv requires a block offset, old filename, new filename");

    return 0;
}
int fat_mv_run(struct fun_context *fctx)
{
    return 0;
}


int fat_rm_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3)
        ERR_RETURN("fat_rm requires a block offset and filename");

    return 0;
}
int fat_rm_run(struct fun_context *fctx)
{
    return 0;
}

int fw_create_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("fw_create requires a filename");

    return 0;
}
int fw_create_run(struct fun_context *fctx)
{
    return 0;
}

int fw_add_local_file_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4)
        ERR_RETURN("fw_add_local_file requires a firmware filename, filename, and file with the contents");

    return 0;
}
int fw_add_local_file_run(struct fun_context *fctx)
{
    return 0;
}

int mbr_write_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2)
        ERR_RETURN("mbr_write requires an mbr");

    return 0;
}
int mbr_write_run(struct fun_context *fctx)
{
    return 0;
}
