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

#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

struct tm;

int timestamp_to_tm(const char *timestamp, struct tm *tmp);
const char *get_creation_timestamp();
void set_last_error(const char *fmt, ...);
const char *last_error();

int hex_to_bytes(const char *str, uint8_t *bytes, size_t maxbytes);
int bytes_to_hex(const uint8_t *bytes, char *str, size_t byte_count);

int archive_filename_to_resource(const char *name, char *result, size_t maxlength);

bool will_be_regular_file(const char *path);

extern bool fwup_verbose;

#define NUM_ELEMENTS(X) (sizeof(X) / sizeof(X[0]))

#define ERR_CLEANUP() do { rc = -1; goto cleanup; } while (0)
#define ERR_CLEANUP_MSG(MSG, ...) do { set_last_error(MSG, ## __VA_ARGS__); rc = -1; goto cleanup; } while (0)

#define OK_OR_CLEANUP(WORK) do { if ((WORK) < 0) ERR_CLEANUP(); } while (0)

#define ERR_RETURN(MSG, ...) do { set_last_error(MSG, ## __VA_ARGS__); return -1; } while (0)
#define OK_OR_RETURN(WORK) do { if ((WORK) < 0) return -1; } while (0)
#define OK_OR_RETURN_MSG(WORK, MSG, ...) do { if ((WORK) < 0) ERR_RETURN(MSG, ## __VA_ARGS__); } while (0)

#define INFO(MSG, ...) do { if (fwup_verbose) fprintf(stderr, MSG, ## __VA_ARGS__); } while (0)

#endif // UTIL_H
