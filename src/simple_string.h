/*
 * Copyright 2016-2017 Frank Hunleth
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

#ifndef SIMPLE_STRING_H
#define SIMPLE_STRING_H

#include <stdarg.h>

/**
 * This is a VERY simple C string implementation to aid in what little
 * string manipulation we do in fwup.
 */
struct simple_string {
    char *str; // Must be freed when done.
    char *p;
    char *end;
};

void simple_string_init(struct simple_string *s);
void ssprintf(struct simple_string *s, const char *format, ...);
void ssvprintf(struct simple_string *s, const char *format, va_list ap);
void ssappend(struct simple_string *s, const char *str);

#endif // SIMPLE_STRING_H
