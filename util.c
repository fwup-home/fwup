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

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static const char *last_error_message = NULL;

void set_now_time()
{
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    if (tmp == NULL) {
        perror("localtime");
        exit(1);
    }

    char outstr[200];
    strftime(outstr, sizeof(outstr), "%a, %d %b %Y %T %z", tmp);
    setenv("NOW", outstr, 1);
}

/**
 * @brief Our errno!
 * @param msg
 */
void set_last_error(const char *msg)
{
    last_error_message = msg;
}

const char *last_error()
{
    return last_error_message;
}
