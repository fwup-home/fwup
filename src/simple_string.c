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

#include "simple_string.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void simple_string_init(struct simple_string *s)
{
    static const size_t starting_size = 4096;
    s->str = malloc(starting_size);
    if (s->str) {
        s->p = s->str;
        s->end = s->str + starting_size;

        // NULL terminate
        s->str[0] = '\0';
    } else {
        s->p = s->end = NULL;
    }
}

static void simple_string_enlarge(struct simple_string *s)
{
    ptrdiff_t len = s->end - s->str;
    ptrdiff_t offset = s->p - s->str;
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

void ssprintf(struct simple_string *s, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    ssvprintf(s, format, ap);

    va_end(ap);
}

void ssvprintf(struct simple_string *s, const char *format, va_list ap)
{
    va_list aq;
    va_copy(aq, ap);
    while (s->str) {
        int max_len = s->end - s->p - 1;
        int n = vsnprintf(s->p, max_len, format, aq);

        // Success is no truncation.

        // Normally this is indicated with n == max_len, but with mingw it
        // is indicated by n < 0.
        if (n >= 0 && n < max_len) {
            s->p += n;
            break;
        }

        // See the vsnprintf documentation that states that aq is invalid after
        // the call. To make it valid again, make a copy of the argument list
        // again.
        va_end(aq);
        va_copy(aq, ap);

        // Retry with a larger buffer
        simple_string_enlarge(s);
    }
    va_end(aq);
}

void ssappend(struct simple_string *s, const char *str)
{
    int len = strlen(str) + 1;
    while (s->str && (s->end - s->p) < len)
        simple_string_enlarge(s);

    if (s->str) {
        memcpy(s->p, str, len);
        s->p += len - 1;
    }
}
