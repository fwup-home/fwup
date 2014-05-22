
#include "confuse.h"
#include <string.h>
#include <stdlib.h>
#include <archive.h>
#include <archive_entry.h>

struct function_info {
    cfg_opt_t *opt;
    int argc;
    char **argv;
    struct function_info *next;
};

static struct function_info *functions = 0;

static void add_function(cfg_opt_t *opt, int argc, const char **argv)
{
    struct function_info *info = (struct function_info *) malloc(sizeof(struct function_info));
    info->opt = opt;
    info->argc = argc;
    if (argc > 0) {
        info->argv = (char **) malloc(argc * sizeof(char *));
        int i;
        for (i = 0; i < argc; i++)
            info->argv[i] = strdup(argv[i]);
    } else
        info->argv = 0;

    info->next = functions;
    functions = info;
}

static int lookup_function(const cfg_opt_t *opt, int *argc, char ***argv)
{
    struct function_info *f = functions;
    while (f) {
        if (opt == f->opt) {
            *argc = f->argc;
            *argv = f->argv;
            return 1;
        }
        f = f->next;
    }
    return 0;
}

static void free_functions()
{
    struct function_info *f = functions;
    functions = 0;

    while (f) {
        int i;
        for (i = 0; i < f->argc; i++)
            free(f->argv[i]);
        free(f->argv);
        struct function_info *next = f->next;
        free(f);
        f = next;
    }
}

void print_func(cfg_opt_t *opt, unsigned int index, FILE *fp)
{
    int argc;
    char **argv;
    if (!lookup_function(opt, &argc, &argv)) {
        fprintf(fp, "%s(?)", opt->name);
        return;
    }

    fprintf(fp, "%s(", opt->name);
    if (argc > 0) {
        fprintf(fp, "%s", argv[0]);

        int i;
        for (i = 1; i < argc; i++)
            fprintf(fp, ",%s", argv[i]);
    }
    fprintf(fp, ")");
}

/* function callback
 */
static int cb_func(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    add_function(opt, argc, argv);
    cfg_opt_set_print_func(opt, print_func);

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

static int cb_validate_file_resource(cfg_t *cfg, cfg_opt_t *opt)
{
    // This is called for each file-resource, so we only need to
    // validate the last one.
    cfg_t *sec = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
    if(!sec)
    {
        cfg_error(cfg, "section is NULL!?");
        return -1;
    }
    const char *path = cfg_getstr(sec, "host-path");
    if (!path) {
        cfg_error(cfg, "host-path must be set for file-report '%s'", cfg_title(sec));
        return -1;
    }

    cfg_setstr(sec, "sha256", "abcdefg");
    cfg_setint(sec, "length", 1234);
    return 0;
}

int main(int argc, char **argv)
{
    cfg_t *cfg;
    int ret;
    static cfg_opt_t file_resource_opts[] = {
        CFG_STR("host-path", 0, CFGF_NONE),
        CFG_INT("length", 0, CFGF_NONE),
        CFG_STR("sha256", 0, CFGF_NONE),
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

    static cfg_opt_t update_on_event_opts[] = {
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
        CFG_SEC("on-init", update_on_event_opts, CFGF_NONE),
        CFG_SEC("on-finish", update_on_event_opts, CFGF_NONE),
        CFG_SEC("on-error", update_on_event_opts, CFGF_NONE),
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
        CFG_STR("meta-creation-date", 0, CFGF_NONE),

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

    cfg = cfg_init(opts, 0);

    /* set a validating callback function for bookmark sections */
    cfg_set_validate_func(cfg, "file-resource", &cb_validate_file_resource);

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
        char *configtxt;
        size_t configtxt_len;
        FILE *fp = open_memstream(&configtxt, &configtxt_len);
        cfg_print(cfg, fp);
        fclose(fp);

        struct archive *a = archive_write_new();
        archive_write_set_format_zip(a);
        if (archive_write_open_filename(a, "test.zip") != ARCHIVE_OK) {
            fprintf(stderr, "Error writing to .zip file");
            return 3;
        }
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, "config.txt");
        archive_entry_set_size(entry, configtxt_len);
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(a, entry);
        archive_write_data(a, configtxt, configtxt_len);
        archive_entry_free(entry);

        archive_write_close(a);
        archive_write_free(a);
        free(configtxt);
    }

    free_functions();
    cfg_free(cfg);
    return 0;
}

