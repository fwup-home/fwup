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

#include <confuse.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include <archive.h>
#include <archive_entry.h>
#include "sha2.h"

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
    (void) index;

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
    (void) cfg;

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

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        cfg_error(cfg, "Cannot open '%s'", path);
        return -1;
    }

    SHA256_CTX ctx256;
    char buffer[1024];
    size_t len = fread(buffer, 1, sizeof(buffer), fp);
    size_t total = len;
    while (len > 0) {
        SHA256_Update(&ctx256, (unsigned char*) buffer, len);
        total += len;
        len = fread(buffer, 1, sizeof(buffer), fp);
    }
    char digest[SHA256_DIGEST_STRING_LENGTH];
    SHA256_End(&ctx256, digest);

    cfg_setstr(sec, "sha256", digest);
    cfg_setint(sec, "length", total);
    return 0;
}

static void set_now_time()
{
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    if (tmp == NULL) {
        perror("localtime");
        exit(1);
    }

    char outstr[200];
    strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z", tmp);
    setenv("NOW", outstr, 1);
}

// Global options
static bool numeric_progress = false;
static bool quiet = false;

//FIXME!!
#define PACKAGE_NAME "fwup"
#define PACKAGE_VERSION "0.1"

static void print_version()
{
    fprintf(stderr, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
}

static void print_usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [options] [path]\n", argv0);
    fprintf(stderr, "  -d <Device file for the memory card>\n");
    fprintf(stderr, "  -f   Run SDCard auto-detection and print the device path\n");
    fprintf(stderr, "  -n   Report numeric progress\n");
    fprintf(stderr, "  -o <Offset from the beginning of the memory card>\n");
    fprintf(stderr, "  -p   Report progress (default)\n");
    fprintf(stderr, "  -q   Quiet\n");
    fprintf(stderr, "  -r   Read from the memory card\n");
    fprintf(stderr, "  -s <Amount to read/write>\n");
    fprintf(stderr, "  -t   Run the TRIM command on the memory card before copying\n");
    fprintf(stderr, "  -v   Print out the version and exit\n");
    fprintf(stderr, "  -w   Write to the memory card (default)\n");
    fprintf(stderr, "  -y   Accept automatically found memory card\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The [path] specifies the location of the image to copy to or from\n");
    fprintf(stderr, "the memory card. If it is unspecified or '-', the image will either\n");
    fprintf(stderr, "be read from stdin (-w) or written to stdout (-r).\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The -d argument does not need to be a device file. It can also be a regular file.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Write the file sdcard.img to an automatically detected SD Card:\n");
    fprintf(stderr, "  %s sdcard.img\n", argv0);
    fprintf(stderr, "\n");
    fprintf(stderr, "Read the master boot record (512 bytes @ offset 0) from /dev/sdc:\n");
    fprintf(stderr, "  %s -r -s 512 -o 0 -d /dev/sdc mbr.img\n", argv0);
    fprintf(stderr, "\n");
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
        CFG_INT("block-offset", 0, CFGF_NONE),
        CFG_INT("block-count", 0, CFGF_NONE),
        CFG_INT("type", 0, CFGF_NONE),
        CFG_END()
    };
    static cfg_opt_t mbr_opts[] = {
        CFG_STR("bootstrap-code-path", 0, CFGF_NONE),
        CFG_SEC("partition", mbr_partition_opts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_END()
    };

#define CFG_ON_EVENT_FUNCTIONS \
    CFG_FUNC("raw_write", cb_func), \
    CFG_FUNC("fat_mkfs", cb_func), \
    CFG_FUNC("fat_write", cb_func), \
    CFG_FUNC("fat_mv", cb_func), \
    CFG_FUNC("fat_rm", cb_func), \
    CFG_FUNC("fw_create", cb_func), \
    CFG_FUNC("fw_add_local_file", cb_func), \
    CFG_FUNC("mbr_write", cb_func)

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
        CFG_BOOL("verify-on-the-fly", cfg_false, CFGF_NONE),
        CFG_BOOL("require-unmounted-destination", cfg_false, CFGF_NONE),
        CFG_SEC("on-init", update_on_event_opts, CFGF_NONE),
        CFG_SEC("on-finish", update_on_event_opts, CFGF_NONE),
        CFG_SEC("on-error", update_on_event_opts, CFGF_NONE),
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

    set_now_time();

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
        archive_entry_set_pathname(entry, "meta.conf");
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

