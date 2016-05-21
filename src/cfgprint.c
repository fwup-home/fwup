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
#include <stdlib.h>

// This implementation is essentially a copy/pasted version of the default cfg_print
// from libconfuse with the following changes:
//
//   1. Output is to a string rather than a file handle. This removes the
//      need for using open_memstream(). That turned out to not be portable.
//   2. Remove unset attributes are not added to the string
//   3. Remove extra spaces and indentation
//   4. Remove custom printers (we didn't used them anyway)

#define is_set(f, x) (((f) & (x)) == (f))

// Sigh.
struct simple_string {
    char *str;
    char *p;
    char *end;
};

static void simple_string_init(struct simple_string *s)
{
    static const size_t starting_size = 4096;
    s->str = malloc(starting_size);
    if (s->str) {
        s->p = s->str;
        s->end = s->str + starting_size;
    } else {
        s->p = s->end = NULL;
    }
}

static void simple_string_enlarge(struct simple_string *s)
{
    size_t len = s->end - s->str;
    size_t offset = s->p - s->str;
    len *= 2;
    char *new_str = realloc(s->str, len);
    if (new_str) {
        s->str = new_str;
        s->p = new_str + offset;
        s->end = new_str + len;
    } else {
        free(s->str);
        s->str = s->p = s->end = NULL;
    }
}

static void ssprintf(struct simple_string *s, const char *format, ...)
{
    va_list ap;
    while (s->str) {
        va_start(ap, format);
        int max_len = s->end - s->p;
        int n = vsnprintf(s->p, max_len, format, ap);
        va_end(ap);

        // vsnprintf failed
        if (n < 0)
            return;

        // Success
        if (n < max_len) {
            s->p += n;
            return;
        }

        // Retry with a larger buffer
        simple_string_enlarge(s);
    }
}

static void fwup_cfg_print(cfg_t *cfg, struct simple_string *s);

static void fwup_cfg_opt_nprint_var(cfg_opt_t *opt, unsigned int index, struct simple_string *s)
{
    switch (opt->type) {
    case CFGT_INT:
        ssprintf(s, "%ld", cfg_opt_getnint(opt, index));
        break;

    case CFGT_FLOAT:
        ssprintf(s, "%f", cfg_opt_getnfloat(opt, index));

    case CFGT_STR:
    {
        const char *str = cfg_opt_getnstr(opt, index);
        ssprintf(s, "\"");
        while (str && *str) {
            if (*str == '"')
                ssprintf(s, "\\\"");
            else if (*str == '\\')
                ssprintf(s, "\\\\");
            else
                ssprintf(s, "%c", *str);
            str++;
        }
        ssprintf(s, "\"");
        break;
    }

    case CFGT_BOOL:
        ssprintf(s, "%s", cfg_opt_getnbool(opt, index) ? "true" : "false");
        break;

    case CFGT_NONE:
    case CFGT_SEC:
    case CFGT_FUNC:
    case CFGT_PTR:
        break;
    }
}

static void fwup_cfg_opt_print(cfg_opt_t *opt, struct simple_string *s)
{
    if (!opt)
        return;

    if (opt->type == CFGT_SEC) {
        cfg_t *sec;
        unsigned int i;

        for (i = 0; i < cfg_opt_size(opt); i++) {
            sec = cfg_opt_getnsec(opt, i);
            if (is_set(CFGF_TITLE, opt->flags))
                ssprintf(s, "%s \"%s\" {\n", opt->name, cfg_title(sec));
            else
                ssprintf(s, "%s {\n", opt->name);
            fwup_cfg_print(sec, s);
            ssprintf(s, "}\n");
        }
    } else if (opt->type != CFGT_FUNC && opt->type != CFGT_NONE) {
        if (is_set(CFGF_LIST, opt->flags)) {
            ssprintf(s, "%s = {", opt->name);

            if (opt->nvalues) {
                fwup_cfg_opt_nprint_var(opt, 0, s);
                for (unsigned int i = 1; i < opt->nvalues; i++) {
                    ssprintf(s, ", ");
                    fwup_cfg_opt_nprint_var(opt, i, s);
                }
            }

            ssprintf(s, "}\n");
        } else {
            // if not set, don't print
            if (opt->simple_value.ptr) {
                if (opt->type == CFGT_STR && *opt->simple_value.string == 0)
                    return;
            } else {
                if (cfg_opt_size(opt) == 0 || (opt->type == CFGT_STR && (opt->values[0]->string == 0 ||
                                                                         opt->values[0]->string[0] == 0)))
                    return;
            }
            ssprintf(s, "%s=", opt->name);
            fwup_cfg_opt_nprint_var(opt, 0, s);
            ssprintf(s, "\n");
        }
    }
}

static void fwup_cfg_print(cfg_t *cfg, struct simple_string *s)
{
    for (int i = 0; cfg->opts[i].name && s->str; i++)
        fwup_cfg_opt_print(&cfg->opts[i], s);
}

/**
 * @brief Turn the config into a string
 * @param cfg the config
 * @param result a pointer to the string is returned (must be freed)
 * @return the string length
 */
int fwup_cfg_to_string(cfg_t *cfg, char **result)
{
    struct simple_string s;
    simple_string_init(&s);

    fwup_cfg_print(cfg, &s);

    *result = s.str;
    return s.p - s.str;
}
