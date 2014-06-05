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

#include "fwup_list.h"
#include <confuse.h>
#include <stdlib.h>
#include <string.h>

#include "cfgfile.h"
#include "util.h"

static int strsort(const void *a, const void *b)
{
    return strcmp(*(const char **) a, *(const char **) b);
}

static int list_tasks(cfg_t *cfg)
{
    size_t i;
    cfg_opt_t *opt = cfg_getopt(cfg, "task");
    if (!opt)
        ERR_RETURN("Firmware file missing task section");

    const char *tasks[opt->nvalues];
    for (i = 0; i < opt->nvalues; i++) {
        cfg_t *sec = cfg_opt_getnsec(opt, i);
        tasks[i] = cfg_title(sec);
    }
    qsort(tasks, opt->nvalues, sizeof(const char *), strsort);

    for (i = 0; i < opt->nvalues; i++)
        printf("%s\n", tasks[i]);

    return 0;
}

int fwup_list(const char *fw_filename)
{
    cfg_t *cfg = NULL;
    int rc = 0;

    OK_OR_CLEANUP(cfgfile_parse_fw_meta_conf(fw_filename, &cfg));

    OK_OR_CLEANUP(list_tasks(cfg));

cleanup:
    if (cfg)
        cfgfile_free(cfg);
    return rc;
}
