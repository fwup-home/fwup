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
#include "../config.h"

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

void format_pretty_size(off_t amount, char *out, size_t out_size);

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
#define CHECK_ARG_UINT64_MAX(ARG, MAX_VAL, MSG) do { errno=0; unsigned long long int val = strtoull(ARG, NULL, 0); if (errno != 0 || val > (MAX_VAL)) ERR_RETURN(MSG); } while (0)

#ifdef __GNUC__
#define FWUP_ERR_ATTRS __attribute__ ((__noreturn__, __format__ (__printf__, 2, 3)))
#define FWUP_WARN_ATTRS __attribute__ ((__format__ (__printf__, 1, 2)))
#else
#define FWUP_ERR_ATTRS
#endif

// These are similar to functions provided by err.h, but they output in the framed
// format when the user specifies --framing.
void fwup_err(int status, const char *format, ...) FWUP_ERR_ATTRS;
void fwup_errx(int status, const char *format, ...) FWUP_ERR_ATTRS;
void fwup_warnx(const char *format, ...) FWUP_WARN_ATTRS;

#ifndef HAVE_STRPTIME
// Provide a prototype for strptime if using the version in the 3rdparty directory.
char* strptime(const char *buf, const char *fmt, struct tm *tm);
#endif

#ifdef _WIN32
// Assume that all windows platforms are little endian
#define TO_BIGENDIAN16(X) _byteswap_ushort(X)
#define FROM_BIGENDIAN16(X) _byteswap_ushort(X)
#define TO_BIGENDIAN32(X) _byteswap_ulong(X)
#define FROM_BIGENDIAN32(X) _byteswap_ulong(X)
#else
// Other platforms have htons and ntohs without pulling in another library
#include <arpa/inet.h>
#define TO_BIGENDIAN16(X) htons(X)
#define FROM_BIGENDIAN16(X) ntohs(X)
#define TO_BIGENDIAN32(X) htonl(X)
#define FROM_BIGENDIAN32(X) ntohl(X)
#endif

#define FRAMING_TYPE_SUCCESS  "OK"
#define FRAMING_TYPE_ERROR    "ER"
#define FRAMING_TYPE_WARNING  "WA"
#define FRAMING_TYPE_PROGRESS "PR"

// Send output to the terminal based on the framing options
void fwup_output(const char *type, uint16_t code, const char *str);

#ifndef HAVE_PREAD
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
#endif

#ifndef HAVE_PWRITE
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n);
#endif

// Getting and setting the environment

// Ideally setenv() is available. If not, provide an implementation.
#define get_environment getenv
#ifdef HAVE_SETENV
// If setenv is available, use getenv/setenv to manage variables
// The "1" means to update the VALUE if NAME is already in the environment.
#define set_environment(NAME, VALUE) setenv(NAME, VALUE, 1)
#else
int set_environment(const char *key, const char *value);
#endif

// On Win32, if open(2) isn't called with O_BINARY, the results are very
// unintuitive for anyone used to Linux development.
#ifdef _WIN32
#define O_WIN32_BINARY O_BINARY
#else
#define O_WIN32_BINARY 0
#endif

#endif // UTIL_H
