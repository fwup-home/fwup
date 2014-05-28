#include "functions.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

static int raw_write_validate(int argc, const char **argv);
static int raw_write_run(int argc, const char **argv);
static int fat_mkfs_validate(int argc, const char **argv);
static int fat_write_validate(int argc, const char **argv);
static int fat_mv_validate(int argc, const char **argv);
static int fat_rm_validate(int argc, const char **argv);
static int fw_create_validate(int argc, const char **argv);
static int fw_add_local_file_validate(int argc, const char **argv);
static int mbr_write_validate(int argc, const char **argv);

struct fun_info {
    const char *name;
    int (*validate)(int argc, const char **argv);
    int (*run)(int argc, const char **argv);
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
 * @param fun the function name
 * @param argc argument count
 * @param argv arguments
 * @return 0 if ok
 */
int fun_validate(int argc, const char **argv)
{
    struct fun_info *fun = lookup(argc, argv);
    if (!fun)
        return -1;

    return fun->validate(argc, argv);
}

/**
 * @brief Run a function
 * @param fun the function name
 * @param argc argument count
 * @param argv arguments
 * @return 0 if ok
 */
int fun_run(int argc, const char **argv)
{
    struct fun_info *fun = lookup(argc, argv);
    if (!fun)
        return -1;

    return fun->run(argc, argv);
}

int raw_write_validate(int argc, const char **argv)
{
    if (argc != 2) {
        set_last_error("raw_write requires a block offset");
        return -1;
    }

    int offset = strtoul(argv[1], 0, 0);
    if (offset < 0) {
        set_last_error("block offset should be non-negative");
        return -1;
    }

    return 0;
}

int raw_write_run(int argc, const char **argv)
{

    return 0;
}

int fat_mkfs_validate(int argc, const char **argv)
{
    if (argc != 3) {
        set_last_error("fat_mkfs requires a block offset and block count");
        return -1;
    }

    return 0;
}

int fat_write_validate(int argc, const char **argv)
{
    if (argc != 3) {
        set_last_error("fat_write requires a block offset and destination filename");
        return -1;
    }

    return 0;
}
int fat_mv_validate(int argc, const char **argv)
{
    if (argc != 4) {
        set_last_error("fat_mv requires a block offset, old filename, new filename");
        return -1;
    }

    return 0;
}
int fat_rm_validate(int argc, const char **argv)
{
    if (argc != 3) {
        set_last_error("fat_rm requires a block offset and filename");
        return -1;
    }

    return 0;
}
int fw_create_validate(int argc, const char **argv)
{
    if (argc != 2) {
        set_last_error("fw_create requires a filename");
        return -1;
    }

    return 0;
}
int fw_add_local_file_validate(int argc, const char **argv)
{
    if (argc != 4) {
        set_last_error("fw_add_local_file requires a firmware filename, filename, and file with the contents");
        return -1;
    }

    return 0;
}
int mbr_write_validate(int argc, const char **argv)
{
    if (argc != 2) {
        set_last_error("mbr_write requires an mbr");
        return -1;
    }

    return 0;
}
