/*
 * Copyright 2016 Frank Hunleth
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

#include "cfgprint.h"
#include "errno.h"

#include <string.h>

// This implementation is copy/pasted version of the default cfg_print from
// libconfuse except that it leaves out attributes that we don't want
// printed to the meta.conf in the .fw file.

#define is_set(f, x) (((f) & (x)) == (f))

static void fwup_cfg_indent(FILE *fp, int indent)
{
    while (indent--)
        fprintf(fp, "  ");
}

static int fwup_cfg_opt_print_indent(cfg_opt_t *opt, FILE *fp, int indent)
{
    if (!opt || !fp) {
        errno = EINVAL;
        return -1;
    }

    if (opt->type == CFGT_SEC) {
        cfg_t *sec;
        unsigned int i;

        for (i = 0; i < cfg_opt_size(opt); i++) {
            sec = cfg_opt_getnsec(opt, i);
            fwup_cfg_indent(fp, indent);
            if (is_set(CFGF_TITLE, opt->flags))
                fprintf(fp, "%s \"%s\" {\n", opt->name, cfg_title(sec));
            else
                fprintf(fp, "%s {\n", opt->name);
            fwup_cfg_print_indent(sec, fp, indent + 1);
            fwup_cfg_indent(fp, indent);
            fprintf(fp, "}\n");
        }
    } else if (opt->type != CFGT_FUNC && opt->type != CFGT_NONE) {
        if (is_set(CFGF_LIST, opt->flags)) {
            fwup_cfg_indent(fp, indent);
            fprintf(fp, "%s = {", opt->name);

            if (opt->nvalues) {
                unsigned int i;

                if (opt->pf)
                    opt->pf(opt, 0, fp);
                else
                    cfg_opt_nprint_var(opt, 0, fp);
                for (i = 1; i < opt->nvalues; i++) {
                    fprintf(fp, ", ");
                    if (opt->pf)
                        opt->pf(opt, i, fp);
                    else
                        cfg_opt_nprint_var(opt, i, fp);
                }
            }

            fprintf(fp, "}");
        } else {
            if (strcmp(opt->name, "__unknown")) {
            fwup_cfg_indent(fp, indent);
            /* comment out the option if is not set */
            if (opt->simple_value.ptr) {
                if (opt->type == CFGT_STR && *opt->simple_value.string == 0)
                    fprintf(fp, "# ");
            } else {
                if (cfg_opt_size(opt) == 0 || (opt->type == CFGT_STR && (opt->values[0]->string == 0 ||
                                             opt->values[0]->string[0] == 0)))
                    fprintf(fp, "# ");
            }
            fprintf(fp, "%s=", opt->name);
            if (opt->pf)
                opt->pf(opt, 0, fp);
            else
                cfg_opt_nprint_var(opt, 0, fp);
            }
        }

        if (strcmp(opt->name, "__unknown"))
            fprintf(fp, "\n");
    } else if (opt->pf) {
        fwup_cfg_indent(fp, indent);
        opt->pf(opt, 0, fp);
        fprintf(fp, "\n");
    }

    return CFG_SUCCESS;
}

int fwup_cfg_print_indent(cfg_t *cfg, FILE *fp, int indent)
{
    int i, result = CFG_SUCCESS;

    for (i = 0; cfg->opts[i].name; i++)
        result += fwup_cfg_opt_print_indent(&cfg->opts[i], fp, indent);

    return result;
}
