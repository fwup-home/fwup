#include "fwup_list.h"
#include <confuse.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "cfgfile.h"
#include "util.h"

static int strsort(const void *a, const void *b)
{
    return strcmp(*(const char **) a, *(const char **) b);
}

static void list_tasks(cfg_t *cfg)
{
    size_t i;
    cfg_opt_t *opt = cfg_getopt(cfg, "update");
    const char *tasks[opt->nvalues];
    for (i = 0; i < opt->nvalues; i++) {
        cfg_t *sec = cfg_opt_getnsec(opt, i);
        tasks[i] = cfg_title(sec);
    }
    qsort(tasks, opt->nvalues, sizeof(const char *), strsort);

    for (i = 0; i < opt->nvalues; i++)
        printf("%s\n", tasks[i]);
}

void fwup_list(const char *fw_filename)
{
    cfg_t *cfg;
    if (cfgfile_parse_fw_meta_conf(fw_filename, &cfg) < 0)
        errx(EXIT_FAILURE, "%s", last_error());

    list_tasks(cfg);

    cfgfile_free(cfg);
}
