#ifndef CFGPRINT_H
#define CFGPRINT_H

#include <confuse.h>
#include "simple_string.h"

int fwup_cfg_to_string(cfg_t *cfg, char **result);

void fwup_cfg_opt_to_string(cfg_opt_t *opt, struct simple_string *s);

#endif // CFGPRINT_H
