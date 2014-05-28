#include "fwup_create.h"
#include "cfgfile.h"
#include "util.h"
#include "sha2.h"

#include <err.h>
#include <stdlib.h>

static void compute_file_metadata(cfg_t *cfg)
{
    cfg_t *sec;
    int i;

    while ((sec = cfg_getnsec(cfg, "file-resource", i++)) != NULL) {
        const char *path = cfg_getstr(sec, "host-path");
        if (!path)
            errx(EXIT_FAILURE, "host-path must be set for file-report '%s'", cfg_title(sec));

        FILE *fp = fopen(path, "rb");
        if (!fp)
            err(EXIT_FAILURE, "Cannot open '%s'", path);

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
    }
}

static void cfg_to_string(cfg_t *cfg, char **output, size_t *len)
{
    FILE *fp = open_memstream(output, &len);
    cfg_print(cfg, fp);
    fclose(fp);
}

void fwup_create(const char *configfile, const char *output_firmware)
{
    cfg_t *cfg;

    set_now_time();

    if (cfgfile_parse_file(configfile, &cfg) < 0)
        errx(EXIT_FAILURE, "Error parsing %s", configfile);

    compute_file_metadata(cfg);

    cfg_print(cfg, stdout);
    cfgfile_free(cfg);
}
