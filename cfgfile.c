#include "cfgfile.h"
#include "mbr.h"
#include "functions.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <archive.h>
#include <archive_entry.h>


/* function callback
 */
static int cb_func(enum fun_context_type ctype, cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    struct fun_context fctx;
    memset(&fctx, 0, sizeof(fctx));
    fctx.type = ctype;


    // Convert to the normal argc/argv
    fctx.argc = argc + 1;
    const char *nargv[fctx.argc];
    nargv[0] = opt->name;
    memcpy(&nargv[1], argv, sizeof(const char *) * argc);
    fctx.argv = nargv;

    if (fun_validate(&fctx) < 0) {
        cfg_error(cfg, last_error());
        return -1;
    }

    char str_argc[5];
    sprintf(str_argc, "%d", fctx.argc);

    cfg_addlist(cfg, "funlist", 2, str_argc, fctx.argv[0]);
    int i;
    for (i = 1; i < fctx.argc; i++)
        cfg_addlist(cfg, "funlist", 1, fctx.argv[i]);

    return 0;
}

static int cb_on_init_func(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    return cb_func(FUN_CONTEXT_INIT, cfg, opt, argc, argv);
}

static int cb_on_finish_func(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    return cb_func(FUN_CONTEXT_FINISH, cfg, opt, argc, argv);
}

static int cb_on_error_func(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    return cb_func(FUN_CONTEXT_ERROR, cfg, opt, argc, argv);
}

static int cb_on_resource_func(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    return cb_func(FUN_CONTEXT_FILE, cfg, opt, argc, argv);
}

static int cb_define(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
    /* at least one parameter is required */
    if(argc != 2) {
        cfg_error(cfg, "Too few parameters for the '%s' function",
                  opt->name);
        return -1;
    }

    // Update the environment. (Overwrite since it is easy for the
    // user to specifya non-overwriting version by supplying a default)
    if (setenv(argv[0], argv[1], 1) < 0) {
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

    return 0;
}

static int cb_validate_mbr(cfg_t *cfg, cfg_opt_t *opt)
{
    // This is called for each mbr, so we only need to
    // validate the last one.
    cfg_t *sec = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
    if(!sec)
    {
        cfg_error(cfg, "section is NULL!?");
        return -1;
    }

    // TODO - validate bootstrap-code-path

    cfg_t *partition;
    int i = 0;
    int found_partitions = 0;

    struct mbr_partition partitions[4];
    memset(partitions, 0, sizeof(partitions));

    while ((partition = cfg_getnsec(sec, "partition", i++)) != NULL) {
        int partition_ix = strtoul(cfg_title(partition), NULL, 0);
        if (partition_ix < 0 || partition_ix >= 4) {
            cfg_error(cfg, "partition must be numbered 0 through 3");
            return -1;
        }
        if (found_partitions & (1 << partition_ix)) {
            cfg_error(cfg, "invalid or duplicate partition number found");
            return -1;
        }
        found_partitions = found_partitions | (1 << partition_ix);

        partitions[partition_ix].partition_type = cfg_getint(partition, "type");
        partitions[partition_ix].block_offset = cfg_getint(partition, "block-offset");
        partitions[partition_ix].block_count = cfg_getint(partition, "block-count");

        if (partitions[partition_ix].partition_type < 0 ||
                partitions[partition_ix].block_offset < 0 ||
                partitions[partition_ix].block_count < 0) {
            cfg_error(cfg, "type, block-offset, and block-count must all be positive and specified");
            return -1;
        }
    }

    if (found_partitions == 0)
        cfg_error(cfg, "empty partition table?");

    if (mbr_verify(partitions) < 0) {
        cfg_error(cfg, last_error());
        return -1;
    }

    return 0;
}


static int cb_validate_on_init(cfg_t *cfg, cfg_opt_t *opt)
{
    (void) cfg;
    (void) opt;
#if 0
    cfg_t *sec = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);

    int i;
    for(i = 0; i < cfg_opt_size(opt); i++)
    {
        sec = cfg_opt_getnsec(opt, i);
        cfg_indent(fp, indent);
        if(is_set(CFGF_TITLE, opt->flags))
            fprintf(fp, "%s \"%s\" {\n", opt->name, cfg_title(sec));
        else
            fprintf(fp, "%s {\n", opt->name);
        cfg_print_indent(sec, fp, indent + 1);
        cfg_indent(fp, indent);
        fprintf(fp, "}\n");
    }

    int i;
    for (i = 0; i < cfg_opt_size(opt); i++) {
        cfg_t *subsec = cfg_opt_getnsec()
        fprintf(stderr, "%s\n", opt->name);
        int j;
        for (j = 0; opt->subopts[j].name; j++) {
            fprintf(stderr, "  %s\n", opt->subopts[j].name);
        }
    }
#endif

#if 0
    cfg_t *sec = cfg_opt_getnsec(opt, cfg_opt_size(opt) - 1);
    if(!sec)
    {
        cfg_error(cfg, "section is NULL!?");
        return -1;
    }


    int i;
    for (i = 0; sec->opts[i].name; i++) {
        cfg_opt_t *funcall = &sec->opts[i];
        if (funcall->type == CFGT_FUNC) {
            int argc;
            char **argv;
            if (lookup_function(funcall, &argc, &argv) < 0) {
                cfg_error(cfg, "What? %s", funcall->name);
                return -1;
            }
            if (fun_validate(argc, argv) < 0) {
                cfg_error(cfg, last_error());
                return -1;
            }
        }
    }
#endif
    return 0;
}

static cfg_opt_t file_resource_opts[] = {
    CFG_STR("host-path", 0, CFGF_NONE),
    CFG_INT("length", 0, CFGF_NONE),
    CFG_STR("sha256", 0, CFGF_NONE),
    CFG_END()
};
static cfg_opt_t mbr_partition_opts[] = {
    CFG_INT("block-offset", -1, CFGF_NONE),
    CFG_INT("block-count", -1, CFGF_NONE),
    CFG_INT("type", -1, CFGF_NONE),
    CFG_END()
};
static cfg_opt_t mbr_opts[] = {
    CFG_STR("bootstrap-code-path", 0, CFGF_NONE),
    CFG_SEC("partition", mbr_partition_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_END()
};

#define CFG_ON_EVENT_FUNCTIONS(CB) \
    CFG_STR_LIST("funlist", 0, CFGF_NONE), \
    CFG_FUNC("raw_write", CB), \
    CFG_FUNC("fat_mkfs", CB), \
    CFG_FUNC("fat_write", CB), \
    CFG_FUNC("fat_mv", CB), \
    CFG_FUNC("fat_rm", CB), \
    CFG_FUNC("fw_create", CB), \
    CFG_FUNC("fw_add_local_file", CB), \
    CFG_FUNC("mbr_write", CB)

static cfg_opt_t update_on_init_opts[] = {
    CFG_ON_EVENT_FUNCTIONS(cb_on_init_func),
    CFG_END()
};
static cfg_opt_t update_on_finish_opts[] = {
    CFG_ON_EVENT_FUNCTIONS(cb_on_finish_func),
    CFG_END()
};
static cfg_opt_t update_on_error_opts[] = {
    CFG_ON_EVENT_FUNCTIONS(cb_on_error_func),
    CFG_END()
};
static cfg_opt_t update_on_resource_opts[] = {
    CFG_STR("verify-on-the-fly", cfg_false, CFGF_NONE),
    CFG_ON_EVENT_FUNCTIONS(cb_on_resource_func),
    CFG_END()
};
static cfg_opt_t update_opts[] = {
    CFG_INT("require-partition1-offset", 0, CFGF_NONE),
    CFG_BOOL("verify-on-the-fly", cfg_false, CFGF_NONE),
    CFG_BOOL("require-unmounted-destination", cfg_false, CFGF_NONE),
    CFG_SEC("on-init", update_on_init_opts, CFGF_NONE),
    CFG_SEC("on-finish", update_on_finish_opts, CFGF_NONE),
    CFG_SEC("on-error", update_on_error_opts, CFGF_NONE),
    CFG_SEC("on-resource", update_on_resource_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_END()
};
cfg_opt_t opts[] = {
    CFG_STR("meta-product", 0, CFGF_NONE),
    CFG_STR("meta-description", 0, CFGF_NONE),
    CFG_STR("meta-version", 0, CFGF_NONE),
    CFG_STR("meta-author", 0, CFGF_NONE),
    CFG_STR("meta-creation-date", 0, CFGF_NONE),

    CFG_STR("require-fwup-version", "0.0", CFGF_NONE),
    CFG_FUNC("define", cb_define),
    CFG_SEC("file-resource", file_resource_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC("mbr", mbr_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
    CFG_SEC("update", update_opts, CFGF_MULTI | CFGF_TITLE),
    //CFG_FUNC("include", &cfg_include),
    CFG_END()
};


int cfgfile_parse_buffer(const char *buffer, cfg_t **cfg)
{
    *cfg = cfg_init(opts, 0);

    /* set a validating callback function for sections */
    cfg_set_validate_func(*cfg, "file-resource", cb_validate_file_resource);
    cfg_set_validate_func(*cfg, "mbr", cb_validate_mbr);

    cfg_set_validate_func(*cfg, "update|on-init", cb_validate_on_init);

    if (cfg_parse_buf(*cfg, buffer) != 0)
        return -1;
    else
        return 0;
}

int cfgfile_parse_file(const char *filename, cfg_t **cfg)
{
    *cfg = cfg_init(opts, 0);

    /* set a validating callback function for sections */
    cfg_set_validate_func(*cfg, "file-resource", cb_validate_file_resource);
    cfg_set_validate_func(*cfg, "mbr", cb_validate_mbr);
    cfg_set_validate_func(*cfg, "update|on-init", cb_validate_on_init);
    if (cfg_parse(*cfg, filename) != 0)
        return -1;
    else
        return 0;
}

static char *read_meta_conf(const char *fw_filename)
{
    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);
    int rc = archive_read_open_filename(a, fw_filename, 16384);
    if (rc != ARCHIVE_OK) {
        set_last_error("Cannot open archive");
        return 0;
    }

    struct archive_entry *ae;
    rc = archive_read_next_header(a, &ae);
    if (rc != ARCHIVE_OK) {
        set_last_error("Error reading archive");
        return 0;
    }

    if (strcmp(archive_entry_pathname(ae), "meta.conf") != 0) {
        set_last_error("Expecting meta.conf to be first file");
        return 0;
    }

    if (!archive_entry_size_is_set(ae)) {
        set_last_error("Expecting meta.conf size to be set");
        return 0;
    }

    ssize_t total_size = archive_entry_size(ae);
    if (total_size < 10 || total_size > 50000) {
        set_last_error("Unexpected meta.conf size");
        return 0;
    }

    char *buffer = (char *) malloc(total_size + 1);
    ssize_t size_left = total_size;
    while (size_left > 0) {
      ssize_t len = archive_read_data(a, &buffer[total_size - size_left], size_left);
      if (len <= 0) {
          set_last_error("Error reading all of meta.conf");
          return 0;
      }
      size_left -= len;
    }
    buffer[total_size] = 0;
    archive_read_free(a);
    return buffer;
}

int cfgfile_parse_fw_meta_conf(const char *filename, cfg_t **cfg)
{
    char *meta_conf = read_meta_conf(filename);
    if (!meta_conf)
        return -1;

    if (cfgfile_parse_buffer(meta_conf, cfg) < 0) {
        set_last_error("Unexpected error parsing meta.conf");
        free(meta_conf);
        return -1;
    }
    free(meta_conf);

    return 0;
}

void cfgfile_free(cfg_t *cfg)
{
    cfg_free(cfg);
}
