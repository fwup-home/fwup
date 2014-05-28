#include "fwup_metadata.h"
#include <confuse.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "cfgfile.h"
#include "util.h"

static void list_one(cfg_t *cfg, const char *key)
{
    cfg_opt_print(cfg_getopt(cfg, key), stdout);
}

static void list_metadata(cfg_t *cfg)
{
    list_one(cfg, "meta-product");
    list_one(cfg, "meta-description");
    list_one(cfg, "meta-version");
    list_one(cfg, "meta-author");
    list_one(cfg, "meta-creation-date");
}

void fwup_metadata(const char *fw_filename)
{
    cfg_t *cfg;
    if (cfgfile_parse_fw_meta_conf(fw_filename, &cfg) < 0)
        errx(EXIT_FAILURE, "%s", last_error());

    list_metadata(cfg);

    cfgfile_free(cfg);
}
