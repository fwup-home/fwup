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
#include <sys/types.h>
#include "config.h"

struct tm;

int timestamp_to_tm(const char *timestamp, struct tm *tmp);
const char *get_creation_timestamp();
void set_last_error(const char *fmt, ...);
const char *last_error();

int hex_to_bytes(const char *str, uint8_t *bytes, size_t maxbytes);
int bytes_to_hex(const uint8_t *bytes, char *str, size_t byte_count);

int archive_filename_to_resource(const char *name, char *result, size_t maxlength);

bool will_be_regular_file(const char *path);
bool file_exists(const char *path);

void format_pretty_size(off_t amount, char *out);

extern bool fwup_verbose;

#define NUM_ELEMENTS(X) (sizeof(X) / sizeof(X[0]))

#define ERR_CLEANUP() do { rc = -1; goto cleanup; } while (0)
#define ERR_CLEANUP_MSG(MSG, ...) do { set_last_error(MSG, ## __VA_ARGS__); rc = -1; goto cleanup; } while (0)

#define OK_OR_CLEANUP(WORK) do { if ((WORK) < 0) ERR_CLEANUP(); } while (0)

#define ERR_RETURN(MSG, ...) do { set_last_error(MSG, ## __VA_ARGS__); return -1; } while (0)
#define OK_OR_RETURN(WORK) do { if ((WORK) < 0) return -1; } while (0)
#define OK_OR_RETURN_MSG(WORK, MSG, ...) do { if ((WORK) < 0) ERR_RETURN(MSG, ## __VA_ARGS__); } while (0)

#define INFO(MSG, ...) do { if (fwup_verbose) fprintf(stderr, MSG, ## __VA_ARGS__); } while (0)

#define ONE_KiB  (1024LL)
#define ONE_MiB  (1024 * ONE_KiB)
#define ONE_GiB  (1024 * ONE_MiB)

// This checks that the argument can be converted to a uint. It is
// non-trivial to suppress compiler warnings.
#define CHECK_ARG_UINT64(ARG, MSG) do { errno=0; unsigned long long int _ = strtoull(ARG, NULL, 0); (void) _; if (errno != 0) ERR_RETURN(MSG); } while (0)

#ifdef HAVE_ERR_H
#include <err.h>
#else
// If err.h doesn't exist, define substitutes.
#define err(STATUS, MSG, ...) do { fprintf(stderr, "fwup: " MSG "\n", ## __VA_ARGS__); exit(STATUS); } while (0)
#define errx(STATUS, MSG, ...) do { fprintf(stderr, "fwup: " MSG "\n", ## __VA_ARGS__); exit(STATUS); } while (0)
#define warn(MSG, ...) do { fprintf(stderr, "fwup: " MSG "\n", ## __VA_ARGS__); } while (0)
#define warnx(MSG, ...) do { fprintf(stderr, "fwup: " MSG "\n", ## __VA_ARGS__); } while (0)
#endif

#ifndef HAVE_STRPTIME
// Provide a prototype for strptime if using the version in the 3rdparty directory.
char* strptime(const char *buf, const char *fmt, struct tm *tm);
#endif

#endif // UTIL_H
