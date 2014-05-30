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

#include "fwup_apply.h"
#include "util.h"
#include "cfgfile.h"

#include <archive.h>
#include <archive_entry.h>
#include <confuse.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "functions.h"

static bool task_is_applicable(cfg_t *task)
{
    // TODO - check that the parameters set on the task are applicable to the current setup.
    (void) task;

    return true;
}

static cfg_t *find_task(cfg_t *cfg, const char *task_prefix)
{
    size_t task_len = strlen(task_prefix);
    cfg_t *task;

    int i;    
    for (i = 0; (task = cfg_getnsec(cfg, "task", i)) != NULL; i++) {
        const char *name = cfg_title(task);
        if (strlen(name) >= task_len &&
                memcmp(task_prefix, name, task_len) == 0 &&
                task_is_applicable(task))
            return task;
    }
    return 0;
}

static int run_event(struct fun_context *fctx, cfg_t *task, const char *event_type, const char *event_parameter)
{
    cfg_t *on_event;
    if (event_parameter)
        on_event = cfg_gettsec(task, event_type, event_parameter);
    else
        on_event = cfg_getsec(task, event_type);

    if (on_event) {
        cfg_opt_t *funlist = cfg_getopt(on_event, "funlist");
        if (funlist) {
            if (fun_run_funlist(fctx, funlist) < 0)
                return -1;
        }
    }
    return 0;
}

struct fwup_data
{
    struct archive *a;
};

static int read_callback(struct fun_context *fctx, const void **buffer, size_t *len, int64_t *offset)
{
    struct fwup_data *p = (struct fwup_data *) fctx->cookie;
    int rc = archive_read_data_block(p->a, buffer, len, offset);
    if (rc == ARCHIVE_EOF) {
        *len = 0;
        *buffer = NULL;
        *offset = 0;
        return 0;
    } else if (rc == ARCHIVE_OK) {
        return 0;
    } else
        ERR_RETURN(archive_error_string(p->a));
}

int fwup_apply(const char *fw_filename, const char *task_prefix, const char *output_filename)
{
    struct fun_context fctx;
    memset(&fctx, 0, sizeof(fctx));
    fctx.output_fd = STDOUT_FILENO;

    if (output_filename) {
        fctx.output_fd = open(output_filename, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (fctx.output_fd < 0)
            ERR_RETURN("Cannot open output");
    }

    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);
    int rc = archive_read_open_filename(a, fw_filename, 16384);
    if (rc != ARCHIVE_OK)
        ERR_RETURN("Cannot open archive");

    struct archive_entry *ae;
    rc = archive_read_next_header(a, &ae);
    if (rc != ARCHIVE_OK)
        ERR_RETURN("Error reading archive");

    if (strcmp(archive_entry_pathname(ae), "meta.conf") != 0)
        ERR_RETURN("Expecting meta.conf to be first file");

    cfg_t *cfg;
    if (cfgfile_parse_fw_ae(a, ae, &cfg) < 0)
        return -1;

    cfg_t *task = find_task(cfg, task_prefix);
    if (task == 0)
        ERR_RETURN("Couldn't find applicable task");

    fctx.type = FUN_CONTEXT_INIT;
    if (run_event(&fctx, task, "on-init", NULL) < 0)
        return -1;

    struct fwup_data private_data;
    fctx.type = FUN_CONTEXT_FILE;
    fctx.cookie = &private_data;
    private_data.a = a;
    fctx.read = read_callback;
    while (archive_read_next_header(a, &ae) == ARCHIVE_OK) {
        const char *filename = archive_entry_pathname(ae);
        if (memcmp(filename, "data/", 5) != 0)
            continue;

        const char *resource_name = &filename[5];
        if (run_event(&fctx, task, "on-resource", resource_name) < 0)
            return -1;
    }

    archive_read_free(a);

    fctx.type = FUN_CONTEXT_FINISH;
    if (run_event(&fctx, task, "on-finish", NULL) < 0)
        return -1;

    close(fctx.output_fd);

    return 0;
}
