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

char *strptime(const char *s, const char *format, struct tm *tm);

static const char *last_error_message = NULL;
static char time_string[200] = {0};

static const char *timestamp_format = "%Y-%m-%dT%H:%M:%SZ";

const char *get_creation_timestamp()
{
    // Ensure that if the creation timestamp is queried more than
    // once that the same string gets returned.
    if (*time_string == '\0') {
        time_t t = time(NULL);
        struct tm *tmp = gmtime(&t);
        if (tmp == NULL) {
            perror("gmtime");
            exit(1);
        }

        strftime(time_string, sizeof(time_string), timestamp_format, tmp);
    }

    return time_string;
}

int timestamp_to_tm(const char *timestamp, struct tm *tmp)
{
    if (strptime(timestamp, timestamp_format, tmp) == NULL)
        ERR_RETURN("error parsing timestamp");
    else
        return 0;
}

void set_now_time()
{
    setenv("NOW", get_creation_timestamp(), 1);
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
