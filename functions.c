#include "functions.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static int raw_write_validate(struct fun_context *fctx);
static int raw_write_run(struct fun_context *fctx);
static int fat_mkfs_validate(struct fun_context *fctx);
static int fat_write_validate(struct fun_context *fctx);
static int fat_mv_validate(struct fun_context *fctx);
static int fat_rm_validate(struct fun_context *fctx);
static int fw_create_validate(struct fun_context *fctx);
static int fw_add_local_file_validate(struct fun_context *fctx);
static int mbr_write_validate(struct fun_context *fctx);

struct fun_info {
    const char *name;
    int (*validate)(struct fun_context *fctx);
    int (*run)(struct fun_context *fctx);
};

static struct fun_info fun_table[] = {
    {"raw_write", raw_write_validate, raw_write_run },
    {"fat_mkfs", fat_mkfs_validate, raw_write_run },
    {"fat_write", fat_write_validate, raw_write_run },
    {"fat_mv", fat_mv_validate, raw_write_run },
    {"fat_rm", fat_rm_validate, raw_write_run },
    {"fw_create", fw_create_validate, raw_write_run },
    {"fw_add_local_file", fw_add_local_file_validate, raw_write_run },
    {"mbr_write", mbr_write_validate, raw_write_run }
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

int raw_write_validate(struct fun_context *fctx)
{
    if (fctx->type != FUN_CONTEXT_FILE) {
        set_last_error("raw_write only usable in on-resource");
        return -1;
    }
    if (fctx->argc != 2) {
        set_last_error("raw_write requires a block offset");
        return -1;
    }

    int offset = strtoul(fctx->argv[1], 0, 0);
    if (offset < 0) {
        set_last_error("block offset should be non-negative");
        return -1;
    }

    return 0;
}

int raw_write_run(struct fun_context *fctx)
{

    return 0;
}

int fat_mkfs_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3) {
        set_last_error("fat_mkfs requires a block offset and block count");
        return -1;
    }

    return 0;
}

int fat_write_validate(struct fun_context *fctx)
{
    if (fctx->type != FUN_CONTEXT_FILE) {
        set_last_error("fat_write only usable in on-resource");
        return -1;
    }
    if (fctx->argc != 3) {
        set_last_error("fat_write requires a block offset and destination filename");
        return -1;
    }

    return 0;
}
int fat_mv_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4) {
        set_last_error("fat_mv requires a block offset, old filename, new filename");
        return -1;
    }

    return 0;
}
int fat_rm_validate(struct fun_context *fctx)
{
    if (fctx->argc != 3) {
        set_last_error("fat_rm requires a block offset and filename");
        return -1;
    }

    return 0;
}
int fw_create_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2) {
        set_last_error("fw_create requires a filename");
        return -1;
    }

    return 0;
}
int fw_add_local_file_validate(struct fun_context *fctx)
{
    if (fctx->argc != 4) {
        set_last_error("fw_add_local_file requires a firmware filename, filename, and file with the contents");
        return -1;
    }

    return 0;
}
int mbr_write_validate(struct fun_context *fctx)
{
    if (fctx->argc != 2) {
        set_last_error("mbr_write requires an mbr");
        return -1;
    }

    return 0;
}
