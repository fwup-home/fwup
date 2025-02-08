/*
 * Copyright 2014-2017 Frank Hunleth
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

// Global options
extern bool fwup_verbose;
extern bool fwup_framing;
extern bool fwup_unsafe;
extern bool fwup_handshake_on_exit;

struct tm;

int timestamp_to_tm(const char *timestamp, struct tm *tmp);
void time_t_to_string(time_t t, char *str, size_t len);
const char *get_creation_timestamp();
time_t get_creation_time_t();

void set_last_error(const char *fmt, ...);
const char *last_error();

int hex_to_bytes(const char *str, uint8_t *bytes, size_t numbytes);
int bytes_to_hex(const uint8_t *bytes, char *str, size_t byte_count);

int archive_filename_to_resource(const char *name, char *result, size_t maxlength);

bool will_be_regular_file(const char *path);
bool file_exists(const char *path);
bool is_regular_file(const char *path);
bool is_device_null(const char *path);

#define NUM_ELEMENTS(X) (sizeof(X) / sizeof(X[0]))

#define ERR_CLEANUP() do { rc = -1; goto cleanup; } while (0)
#define ERR_CLEANUP_MSG(MSG, ...) do { set_last_error(MSG, ## __VA_ARGS__); rc = -1; goto cleanup; } while (0)

#define OK_OR_CLEANUP(WORK) do { if ((WORK) < 0) ERR_CLEANUP(); } while (0)
#define OK_OR_CLEANUP_MSG(WORK, MSG, ...) do { if ((WORK) < 0) ERR_CLEANUP_MSG(MSG, ## __VA_ARGS__); } while (0)

#define ERR_RETURN(MSG, ...) do { set_last_error(MSG, ## __VA_ARGS__); return -1; } while (0)
#define OK_OR_RETURN(WORK) do { if ((WORK) < 0) return -1; } while (0)
#define OK_OR_RETURN_MSG(WORK, MSG, ...) do { if ((WORK) < 0) ERR_RETURN(MSG, ## __VA_ARGS__); } while (0)

#define OK_OR_FAIL(WORK) do { if ((WORK) < 0) fwup_err(EXIT_FAILURE, "Unexpected error at %s:%d", __FILE__, __LINE__); } while (0)

#define INFO(MSG, ...) do { if (fwup_verbose) fwup_warnx(MSG, ## __VA_ARGS__); } while (0)

#define FWUP_BLOCK_SIZE (512)

// See wikipedia for the drama behind the IEC prefixes if this
// bothers you.

// Power of 2 units
#define ONE_KiB  (1024LL)
#define ONE_MiB  (1024 * ONE_KiB)
#define ONE_GiB  (1024 * ONE_MiB)
#define ONE_TiB  (1024 * ONE_GiB)

// Power of 10 units
#define ONE_KB  (1000LL)
#define ONE_MB  (1000 * ONE_KB)
#define ONE_GB  (1000 * ONE_MB)
#define ONE_TB  (1000 * ONE_GB)

// Pretty printing numbers
int format_pretty_auto(off_t amount, char *out, size_t out_size);
int format_pretty(off_t amount, off_t units, char *out, size_t out_size);
const char *units_to_string(off_t units);
off_t find_natural_units(off_t amount);

// This checks that the argument can be converted to a uint. It handles
// a few things:
//   1. Is this a non-empty string?
//   2. Was the string fully converted?
//   3. Did something cause strtoull set errno?
//   4. It discards the return value without triggering a compiler warning.
#define CHECK_ARG_UINT64(ARG, MSG) do { errno=0; const char *nptr = ARG; char *endptr; unsigned long long int _ = strtoull(nptr, &endptr, 0); (void) _; if (errno != 0 || *nptr == '\0' || *endptr != '\0') ERR_RETURN(MSG); } while (0)
#define CHECK_ARG_UINT64_RANGE(ARG, MIN_VAL, MAX_VAL, MSG) do { errno=0; const char *nptr = ARG; char *endptr; unsigned long long int val = strtoull(nptr, &endptr, 0); if (errno != 0 || val > (MAX_VAL) || val < (MIN_VAL) || *nptr == '\0' || *endptr != '\0') ERR_RETURN(MSG); } while (0)

#ifdef __GNUC__
#define FWUP_ERR_ATTRS __attribute__ ((__noreturn__, __format__ (__printf__, 2, 3)))
#define FWUP_WARN_ATTRS __attribute__ ((__format__ (__printf__, 1, 2)))
#define FWUP_EXIT_ATTRS __attribute__ ((__noreturn__))
#else
#define FWUP_ERR_ATTRS
#define FWUP_WARN_ATTRS
#define FWUP_EXIT_ATTRS
#endif

// These are similar to functions provided by err.h, but they output in the framed
// format when the user specifies --framing.
void fwup_err(int status, const char *format, ...) FWUP_ERR_ATTRS;
void fwup_errx(int status, const char *format, ...) FWUP_ERR_ATTRS;
void fwup_warnx(const char *format, ...) FWUP_WARN_ATTRS;
void fwup_exit(int status) FWUP_EXIT_ATTRS;

#define FWUP_MAX_PUBLIC_KEYS 10

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
#define FRAMING_TYPE_INFO     "WN"
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

#ifndef HAVE_MEMMEM
void *memmem(const void *haystack, size_t haystacklen,
             const void *needle, size_t needlelen);
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

// Page aligned memory allocation
void alloc_page_aligned(void **memptr, size_t size);
void free_page_aligned(void *memptr);

int update_relative_path(const char *from_file, const char *filename, char **newpath);

// UUIDs
#define UUID_LENGTH 16
#define UUID_STR_LENGTH 37 /* Includes NULL terminator */

void uuid_to_string(const uint8_t uuid[UUID_LENGTH], char *uuid_str);
int string_to_uuid_me(const char *uuid_str, uint8_t uuid[UUID_LENGTH]);
void calculate_fwup_uuid(const char *data, off_t data_size, char *uuid);

// Endian conversion
void ascii_to_utf16le(const char *input, char *output, size_t len);
void copy_le64(uint8_t *output, uint64_t v);
void copy_le32(uint8_t *output, uint32_t v);
void copy_le16(uint8_t *output, uint16_t v);

// Crypto
#define FWUP_PUBLIC_KEY_LEN 32
#define FWUP_PRIVATE_KEY_LEN 32
#define FWUP_SIGNATURE_LEN 64
#define FWUP_BLAKE2b_256_LEN 32
#define FWUP_BLAKE2b_512_LEN 64

#ifndef FWUP_APPLY_ONLY
int get_random(uint8_t *buf, size_t len);
#endif

#endif // UTIL_H
