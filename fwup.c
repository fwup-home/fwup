
#include "confuse.h"
#include <string.h>
#include <stdlib.h>

void print_func(cfg_opt_t *opt, unsigned int index, FILE *fp)
{
    fprintf(fp, "%s(foo)", opt->name);
}

/* function callback
 */
int cb_func(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    int i;

    /* at least one parameter is required */
    if(argc == 0) {
        cfg_error(cfg, "Too few parameters for the '%s' function",
                  opt->name);
        return -1;
    }

    printf("cb_func() called with %d parameters:\n", argc);
    for(i = 0; i < argc; i++)
        printf("parameter %d: '%s'\n", i, argv[i]);
    return 0;
}

int cb_define(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    /* at least one parameter is required */
    if(argc != 2) {
        cfg_error(cfg, "Too few parameters for the '%s' function",
                  opt->name);
        return -1;
    }

    // Update the environment, but don't overwrite (for now)
    if (setenv(argv[0], argv[1], 0) < 0) {
        cfg_error(cfg, "setenv failed");
        return -1;
    }

    return 0;
}

int cb_validate_bookmark(cfg_t *cfg, cfg_opt_t *opt)
{
    /* only validate the last bookmark */
    cfg_t *sec = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
    if(!sec)
    {
        cfg_error(cfg, "section is NULL!?");
        return -1;
    }
    if(cfg_getstr(sec, "machine") == 0)
    {
        cfg_error(cfg, "machine option must be set for bookmark '%s'",
                  cfg_title(sec));
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    cfg_t *cfg;
    int ret;
    static cfg_opt_t file_resource_opts[] = {
        CFG_STR("host-path", 0, CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t mbr_partition_opts[] = {
        CFG_INT("offset", 0, CFGF_NONE),
        CFG_INT("count", 0, CFGF_NONE),
        CFG_INT("type", 0, CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t mbr_resource_opts[] = {
        CFG_STR("bootstrap-code-path", 0, CFGF_NONE),
        CFG_SEC("partition", mbr_partition_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_END()
    };
    static cfg_opt_t fatfs_file_opts[] = {
        CFG_STR("resource", 0, CFGF_NONE),
        CFG_INT("permissions", 0, CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t fatfs_resource_opts[] = {
        CFG_INT("fat-type", 0, CFGF_NONE),
        CFG_SEC("file", fatfs_file_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_END()
    };

#define CFG_ON_EVENT_FUNCTIONS \
    CFG_FUNC("raw_write", cb_func), \
    CFG_FUNC("fat_write", cb_func), \
    CFG_FUNC("fat_mv", cb_func), \
    CFG_FUNC("fat_rm", cb_func), \
    CFG_FUNC("fs_write", cb_func)

    static cfg_opt_t update_on_init_opts[] = {
        CFG_ON_EVENT_FUNCTIONS,
        CFG_END()
    };
    static cfg_opt_t update_on_finish_opts[] = {
        CFG_ON_EVENT_FUNCTIONS,
        CFG_END()
    };
    static cfg_opt_t update_on_error_opts[] = {
        CFG_ON_EVENT_FUNCTIONS,
        CFG_END()
    };
    static cfg_opt_t update_on_resource_opts[] = {
        CFG_STR("verify-on-the-fly", cfg_false, CFGF_NONE),
        CFG_ON_EVENT_FUNCTIONS,
        CFG_END()
    };
    static cfg_opt_t update_opts[] = {
        CFG_INT("require-partition1-offset", 0, CFGF_NONE),
        CFG_BOOL("require-unmounted-destination", cfg_false, CFGF_NONE),
        CFG_SEC("on-init", update_on_init_opts, CFGF_NONE),
        CFG_SEC("on-finish", update_on_finish_opts, CFGF_NONE),
        CFG_SEC("on-error", update_on_error_opts, CFGF_NONE),
        CFG_SEC("on-resource", update_on_resource_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_END()
    };
    static cfg_opt_t fw_resource_opts[] = {
        CFG_SEC("update", update_opts, CFGF_MULTI | CFGF_TITLE),
        CFG_END()
    };
    cfg_opt_t opts[] = {
        CFG_STR("meta-product", 0, CFGF_NONE),
        CFG_STR("meta-description", 0, CFGF_NONE),
        CFG_STR("meta-version", 0, CFGF_NONE),
        CFG_STR("meta-author", 0, CFGF_NONE),

        CFG_STR("require-fwupdate-version", "0.0", CFGF_NONE),
        CFG_FUNC("define", cb_define),
        CFG_SEC("file-resource", file_resource_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_SEC("mbr-resource", mbr_resource_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_SEC("fatfs-resource", fatfs_resource_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_SEC("fw-resource", fw_resource_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_SEC("update", update_opts, CFGF_MULTI | CFGF_TITLE),
        //CFG_FUNC("include", &cfg_include),
        CFG_END()
    };

    /* for some reason, MS Visual C++ chokes on this (?) */
    printf("Using %s\n\n", confuse_copyright);

    cfg = cfg_init(opts, 0);

    /* set a validating callback function for bookmark sections */
//    cfg_set_validate_func(cfg, "constants", &cb_validate_constants);

    ret = cfg_parse(cfg, argc > 1 ? argv[1] : "fwupdate.conf");
    printf("ret == %d\n", ret);
    if(ret == CFG_FILE_ERROR) {
        perror("fwupdate.conf");
        return 1;
    } else if(ret == CFG_PARSE_ERROR) {
        fprintf(stderr, "parse error\n");
        return 2;
    }

    /* print the parsed values to another file */
    {
        FILE *fp = fopen("test.conf.out", "w");
        //cfg_set_print_func(cfg, "func", print_func);
        cfg_print(cfg, fp);
        fclose(fp);
    }

    cfg_free(cfg);
    return 0;
}

