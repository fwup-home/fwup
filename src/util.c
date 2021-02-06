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
#define _GNU_SOURCE // for vasprintf
#include "util.h"
#include "simple_string.h"
#include "progress.h"

#include <errno.h>
#include <libgen.h>
#include "monocypher.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>

char *strptime(const char *s, const char *format, struct tm *tm);

static char *last_error_message = NULL;
static char time_string[200] = {0};
static time_t now_time = 0;
static const char *timestamp_format = "%Y-%m-%dT%H:%M:%SZ";

const char *get_creation_timestamp()
{
    // Ensure that if the creation timestamp is queried more than
    // once that the same string gets returned.
    if (*time_string != '\0')
        return time_string;

    // Rules for determining archive timestamps
    //
    // 1. Use $SOURCE_DATE_EPOCH if specifed. See
    //    https://reproducible-builds.org/specs/source-date-epoch/
    // 2. Use $NOW if specified. This is the original fwup way of forcing
    //    reproducible builds so we can't break it.
    // 3. Don't try to be deterministic and use the current time.

    const char *source_date_epoch = get_environment("SOURCE_DATE_EPOCH");
    if (source_date_epoch != NULL) {
        now_time = strtoul(source_date_epoch, NULL, 0);
        time_t_to_string(now_time, time_string, sizeof(time_string));
        set_environment("NOW", time_string);
        return time_string;
    }

    const char *now = get_environment("NOW");
    if (now != NULL) {
        // The user specified NOW, so check that it's parsable.
        struct tm tmp;
        if (strptime(now, timestamp_format, &tmp) != NULL) {
            int rc = snprintf(time_string, sizeof(time_string), "%s", now);
            if (rc >= 0 && rc < (int) sizeof(time_string)) {
#ifdef HAVE_TIMEGM
                now_time = timegm(&tmp);
#else
#ifdef _WIN32
                now_time = _mkgmtime(&tmp);
#else
                // mktime is influenced by the local timezone, so this will
                // be wrong, but close.
                now_time = mktime(&tmp);
#endif
#endif
                return time_string;
            }
        }

        INFO("NOW environment variable set, but not in YYYY-MM-DDTHH:MM:SSZ format so ignoring");
    }

    now_time = time(NULL);
    time_t_to_string(now_time, time_string, sizeof(time_string));
    set_environment("NOW", time_string);

    return time_string;
}

time_t get_creation_time_t()
{
    if (now_time == 0)
        get_creation_timestamp();

    return now_time;
}

void time_t_to_string(time_t t, char *str, size_t len)
{
    struct tm *tm_now = gmtime(&t);
    if (tm_now == NULL)
        fwup_err(EXIT_FAILURE, "gmtime");

    strftime(str, len, timestamp_format, tm_now);
}

int timestamp_to_tm(const char *timestamp, struct tm *tmp)
{
    if (strptime(timestamp, timestamp_format, tmp) == NULL)
        ERR_RETURN("error parsing timestamp");
    else
        return 0;
}

/**
 * @brief Our errno!
 * @param msg
 */
void set_last_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    if (last_error_message)
        free(last_error_message);

    // In the completely unlikely case that vasprintf fails, clear
    // the error message pointer
    if (vasprintf(&last_error_message, fmt, ap) < 0)
        last_error_message = NULL;

    va_end(ap);
}

const char *last_error()
{
    return last_error_message ? last_error_message : "none";
}

static uint8_t hexchar_to_int(char c)
{
    switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a':
    case 'A': return 10;
    case 'b':
    case 'B': return 11;
    case 'c':
    case 'C': return 12;
    case 'd':
    case 'D': return 13;
    case 'e':
    case 'E': return 14;
    case 'f':
    case 'F': return 15;
    default: return 255;
    }
}

static char nibble_to_hexchar(uint8_t nibble)
{
    switch (nibble) {
    case 0: return '0';
    case 1: return '1';
    case 2: return '2';
    case 3: return '3';
    case 4: return '4';
    case 5: return '5';
    case 6: return '6';
    case 7: return '7';
    case 8: return '8';
    case 9: return '9';
    case 10: return 'a';
    case 11: return 'b';
    case 12: return 'c';
    case 13: return 'd';
    case 14: return 'e';
    case 15: return 'f';
    default: return '0';
    }
}

static int two_hex_to_byte(const char *str, uint8_t *byte)
{
    uint8_t sixteens = hexchar_to_int(str[0]);
    uint8_t ones = hexchar_to_int(str[1]);
    if (sixteens == 255 || ones == 255)
        ERR_RETURN("Invalid character in hex string");

    *byte = (uint8_t) (sixteens << 4) | ones;
    return 0;
}

int hex_to_bytes(const char *str, uint8_t *bytes, size_t numbytes)
{
    size_t len = strlen(str);
    if (len != numbytes * 2)
        ERR_RETURN("hex string should have length %d, but got %d", numbytes * 2, len);

    while (len) {
        if (two_hex_to_byte(str, bytes) < 0)
            return -1;

        str += 2;
        len -= 2;
        bytes++;
    }
    return 0;
}

int bytes_to_hex(const uint8_t *bytes, char *str, size_t byte_count)
{
    while (byte_count) {
        *str++ = nibble_to_hexchar(*bytes >> 4);
        *str++ = nibble_to_hexchar(*bytes & 0xf);
        bytes++;
        byte_count--;
    }
    *str = '\0';
    return 0;
}

int archive_filename_to_resource(const char *name, char *result, size_t maxlength)
{
    int length;

    // As a matter of convention, everything useful in the archive is stored
    // in the data directory. There are a couple scenarios where it's useful
    // to stuff a file in the root directory of the archive for compatibility
    // with other programs. Those are specified as absolute paths.
    if (memcmp(name, "data/", 5) == 0)
        length = snprintf(result, maxlength, "%s", &name[5]);
    else
        length = snprintf(result, maxlength, "/%s", name);

    if (length < 0 || length >= (int) maxlength)
       ERR_RETURN("Bad path found in archive");

    return 0;
}

/**
 * Return true if the file is already a regular
 * file or it will be one if it is opened without
 * any special flags.
 */
bool will_be_regular_file(const char *path)
{
    bool is_in_dev = false;
#if defined(__APPLE__) || defined(__linux__)
    // Weakly forbid users from creating regular files in /dev, since that's
    // pretty much never their intention. This will eventually cause
    // an error since the code that calls this won't create files unless
    // this function returns true.
    // See https://github.com/fwup-home/fwup/issues/35.

    if (strncmp(path, "/dev/", 5) == 0)
        is_in_dev = true;
#endif

#ifdef _WIN32
    if (strncmp(path, "\\\\.\\", 4) == 0)
        return false;
#endif
    struct stat st;
    int rc = stat(path, &st);
    return (rc == 0 && (st.st_mode & S_IFREG)) || // Existing regular file
           (rc < 0 && errno == ENOENT && !is_in_dev); // Doesn't exist and not in /dev
}

/**
 * Return true if the file exists.
 */
bool file_exists(const char *path)
{
    struct stat st;
    int rc = stat(path, &st);
    return rc == 0;
}

/**
 * Return true if the file exists.
 */
bool is_regular_file(const char *path)
{
    struct stat st;
    int rc = stat(path, &st);
    return rc == 0 && (st.st_mode & S_IFREG);
}

/**
 * Return a string that described the units.
 */
const char *units_to_string(off_t units)
{
    switch (units) {
    case 1: return "bytes";
    case ONE_KiB: return "KiB";
    case ONE_MiB: return "MiB";
    case ONE_GiB: return "GiB";
    case ONE_TiB: return "TiB";
    case ONE_KB: return "KB";
    case ONE_MB: return "MB";
    case ONE_GB: return "GB";
    case ONE_TB: return "TB";
    default: return "?";
    }
}

/**
 * Return the units that should be used for printing the specified
 * amount;
 */
off_t find_natural_units(off_t amount)
{
    if (amount >= ONE_TB) return ONE_TB;
    else if (amount >= ONE_GB) return ONE_GB;
    else if (amount >= ONE_MB) return ONE_MB;
    else if (amount >= ONE_KB) return ONE_KB;
    else return 1;
}

/**
 * Format the specified amount in a human readable way.
 *
 * @param amount the numer
 * @param out    an output buffer
 * @param out_size the size of the output buffer
 * @return the number of bytes written to out
 */
int format_pretty_auto(off_t amount, char *out, size_t out_size)
{
    return format_pretty(amount, find_natural_units(amount), out, out_size);
}

/**
 * Format the specified amount in a human readable way using the
 * specified units.
 *
 * @param amount the number
 * @param units  the units to use for printing the value
 * @param out    an output buffer
 * @param out_size the size of the output buffer
 * @return the number of bytes written to out
 */
int format_pretty(off_t amount, off_t units, char *out, size_t out_size)
{
    double value = ((double) amount) / units;
    const char *units_string = units_to_string(units);

    return snprintf(out, out_size, "%.2f %s", value, units_string);
}

void fwup_err(int status, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    int err = errno;
    struct simple_string s;
    simple_string_init(&s);
    if (fwup_framing) {
        ssvprintf(&s, format, ap);
    } else {
        ssappend(&s, "fwup: ");
        ssvprintf(&s, format, ap);
        ssprintf(&s, ": %s\n", strerror(err));
    }
    fwup_output(FRAMING_TYPE_ERROR, 0, s.str);
    free(s.str);
    fwup_exit(status);

    va_end(ap);
}

void fwup_errx(int status, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    struct simple_string s;
    simple_string_init(&s);
    if (fwup_framing) {
        ssvprintf(&s, format, ap);
    } else {
        ssappend(&s, "fwup: ");
        ssvprintf(&s, format, ap);
        ssappend(&s, "\n");
    }
    fwup_output(FRAMING_TYPE_ERROR, 0, s.str);
    free(s.str);
    fwup_exit(status);

    va_end(ap);
}

void fwup_warnx(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    struct simple_string s;
    simple_string_init(&s);
    if (fwup_framing) {
        ssvprintf(&s, format, ap);
    } else {
        ssappend(&s, "fwup: ");
        ssvprintf(&s, format, ap);
        ssappend(&s, "\n");
    }
    fwup_output(FRAMING_TYPE_WARNING, 0, s.str);
    free(s.str);

    va_end(ap);
}

void fwup_output(const char *type, uint16_t code, const char *str)
{
    size_t len = strlen(str);
    if (fwup_framing) {
        uint32_t be_length = TO_BIGENDIAN32(len + 4);
        uint16_t be_code = TO_BIGENDIAN16(code);
        fwrite(&be_length, 4, 1, stdout);
        fwrite(type, 2, 1, stdout);
        fwrite(&be_code, 2, 1, stdout);
    } else if (fwup_progress_mode == PROGRESS_MODE_NORMAL && len > 0) {
        // Skip a line to avoid the progress bar and then print the message
        fwrite("\r\033[K", 1, 4, stdout);
    }
    if (len)
        fwrite(str, 1, len, stdout);

    fflush(stdout);
}

static void handshake_exit(int status)
{
    char buffer[2] = {0x1a, (char) status};
    if (write(STDOUT_FILENO, buffer, sizeof(buffer)) != sizeof(buffer))
        fprintf(stderr, "fwup: Error sending Ctrl+Z as part of the exit handshake");

    for (;;) {
        char throwaway[4096];
        ssize_t rc = read(STDIN_FILENO, throwaway, sizeof(throwaway));
        if (rc == 0 || (rc < 0 && errno != EINTR))
            break;
    }
}

/*
 * Capture calls to exit to handle the exit handshake for Erlang port integration.
 */
void fwup_exit(int status)
{
    if (fwup_handshake_on_exit)
        handshake_exit(status);

    exit(status);
}

/*
 * Implementations for simple functions that are missing on
 * some operating systems (e.g. Windows).
 */
#ifndef HAVE_PREAD
ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
    if (lseek(fd, offset, SEEK_SET) >= 0)
        return read(fd, buf, count);
    else
        return -1;
}
#endif

#ifndef HAVE_PWRITE
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    if (lseek(fd, offset, SEEK_SET) >= 0)
        return write(fd, buf, count);
    else
        return -1;
}
#endif

#ifndef HAVE_SETENV
int set_environment(const char *key, const char *value)
{
    char *str;
    int len = asprintf(&str, "%s=%s", key, value);
    if (len < 0)
        fwup_err(EXIT_FAILURE, "asprintf");
    return putenv(str);
}
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *s, size_t n)
{
    size_t len = strlen(s);
    if (len < n)
        n = len;
    char *buf = (char *) malloc(n + 1);
    if (buf) {
        memcpy(buf, s, n);
        buf[n] = '\0';
    }
    return buf;
}
#endif

#ifndef HAVE_MEMMEM
void *memmem(const void *haystack, size_t haystacklen,
             const void *needle, size_t needlelen)
{
    const char *p = (const char *) haystack;
    const char *pend = p + haystacklen - needlelen;
    for (;p != pend; p++) {
        if (memcmp(p, needle, needlelen) == 0)
            return (void *) p;
    }
    return NULL;
}
#endif

/*
 * Aligned buffer support
 */
#if HAVE_SYSCONF
static size_t cached_pagesize = 0;
#else
// If no sysconf(), guess the page size
static const size_t cached_pagesize = 4096;
#endif

static inline size_t get_pagesize()
{
#if HAVE_SYSCONF
    // If sysconf() exists, then call it to find the system's page size
    if (cached_pagesize == 0) {
        long rc = sysconf(_SC_PAGESIZE);
        if (rc > 0)
            cached_pagesize = rc;
        else
            cached_pagesize = 4096; // Guess
    }
#endif
    return cached_pagesize;
}

void alloc_page_aligned(void **memptr, size_t size)
{
    size_t pagesize = get_pagesize();

#if HAVE_POSIX_MEMALIGN
    if (posix_memalign(memptr, pagesize, size) < 0)
        fwup_err(EXIT_FAILURE, "posix_memalign %u bytes", (unsigned int) size);
#else
    // Slightly wasteful implementation of posix_memalign
    size_t padding = pagesize + pagesize - 1;
    uint8_t *original = (uint8_t *) malloc(size + padding);
    if (original == NULL)
        fwup_err(EXIT_FAILURE, "malloc %d bytes", (int) (size + padding));

    // Store the original pointer right before the aligned pointer
    uint8_t *aligned = (uint8_t *) (((uint64_t) (original + padding)) & ~(pagesize - 1));
    void **savelocation = (void**) (aligned - sizeof(void*));
    *savelocation = original;
    *memptr = aligned;
#endif
}

void free_page_aligned(void *memptr)
{
#if HAVE_POSIX_MEMALIGN
    free(memptr);
#else
    void **savelocation = ((void **) memptr) - 1;
    void *original = *savelocation;
    free(original);
#endif
}

int update_relative_path(const char *fromfile, const char *filename, char **newpath)
{
    if (filename[0] == '/' ||
            filename[0] == '~') {
        // If absolute then don't modify the path
        *newpath = strdup(filename);
    } else {
        // If relative, make it relative to the specified file.
        char *fromfile_copy = strdup(fromfile);
        char *fromdir = dirname(fromfile_copy);
        if (asprintf(newpath, "%s/%s", fromdir, filename) < 0)
            fwup_err(EXIT_FAILURE, "asprintf");

        free(fromfile_copy);
    }
    return 0;
}

/**
 * Convert a UUID to a string in big endian form.
 *
 * Historical note: fwup always printed UUIDs in big endian. Then GPT support
 * came in with the mixed endian UUIDs. That's why this looks weird and it's
 * hard to change them to be consistent.
 *
 * @param uuid the UUID
 * @param uuid_str a buffer that's UUID_STR_LENGTH long
 */
void uuid_to_string_be(const uint8_t uuid[UUID_LENGTH], char *uuid_str)
{
    sprintf(uuid_str, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
            uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
}

/**
 * Convert a string to a mixed endian UUID
 *
 * @param uuid_str the string
 * @param uuid the resulting UUID
 * @return 0 on success; -1 on error
 */
int string_to_uuid_me(const char *uuid_str, uint8_t uuid[UUID_LENGTH])
{
    // sscanf %02hhx doesn't seem to work everywhere (i.e., Windows), so we
    // have to work harder.

    // 32 digits and 4 hyphens
    if ((strlen(uuid_str) >= 32 + 4) &&
            two_hex_to_byte(&uuid_str[0], &uuid[3]) >= 0 &&
            two_hex_to_byte(&uuid_str[2], &uuid[2]) >= 0 &&
            two_hex_to_byte(&uuid_str[4], &uuid[1]) >= 0 &&
            two_hex_to_byte(&uuid_str[6], &uuid[0]) >= 0 &&
            uuid_str[8] == '-' &&
            two_hex_to_byte(&uuid_str[9], &uuid[5]) >= 0 &&
            two_hex_to_byte(&uuid_str[11], &uuid[4]) >= 0 &&
            uuid_str[13] == '-' &&
            two_hex_to_byte(&uuid_str[14], &uuid[7]) >= 0 &&
            two_hex_to_byte(&uuid_str[16], &uuid[6]) >= 0 &&
            uuid_str[18] == '-' &&
            two_hex_to_byte(&uuid_str[19], &uuid[8]) >= 0 &&
            two_hex_to_byte(&uuid_str[21], &uuid[9]) >= 0 &&
            uuid_str[23] == '-' &&
            two_hex_to_byte(&uuid_str[24], &uuid[10]) >= 0 &&
            two_hex_to_byte(&uuid_str[26], &uuid[11]) >= 0 &&
            two_hex_to_byte(&uuid_str[28], &uuid[12]) >= 0 &&
            two_hex_to_byte(&uuid_str[30], &uuid[13]) >= 0 &&
            two_hex_to_byte(&uuid_str[32], &uuid[14]) >= 0 &&
            two_hex_to_byte(&uuid_str[34], &uuid[15]) >= 0)
        return 0;
    else
        return -1;
}

/**
 * Create a new UUID by hashing the specified data
 *
 * @param data
 * @param data_size
 * @param uuid
 */
void calculate_fwup_uuid(const char *data, off_t data_size, char *uuid)
{
    crypto_blake2b_ctx hash_state;
    crypto_blake2b_general_init(&hash_state, FWUP_BLAKE2b_256_LEN, NULL, 0);

    // fwup's UUID: 2053dffb-d51e-4310-b93b-956da89f9f34
    unsigned char fwup_uuid[UUID_LENGTH] = {0x20, 0x53, 0xdf, 0xfb, 0xd5, 0x1e, 0x43, 0x10, 0xb9, 0x3b, 0x95, 0x6d, 0xa8, 0x9f, 0x9f, 0x34};

    crypto_blake2b_update(&hash_state, fwup_uuid, sizeof(fwup_uuid));
    crypto_blake2b_update(&hash_state, (const unsigned char *) data, (unsigned long long) data_size);

    unsigned char hash[64];
    crypto_blake2b_final(&hash_state, hash);

    // Set version number (RFC 4122) to 5. This really isn't right since we're
    // not using SHA-1, but monocypher doesn't include SHA-1.
    hash[6] = (hash[6] & 0x0f) | 0x50;

    uuid_to_string_be(hash, uuid);
}

/**
 * Convert an ASCII string to UTF16LE
 *
 * @param input The input string
 * @param output Where to store the output
 * @param len The number of characters to convert
 */
void ascii_to_utf16le(const char *input, char *output, size_t len)
{
    while (len) {
        *output++ = *input++;
        *output++ = 0;
        len--;
    }
}

void copy_le64(uint8_t *output, uint64_t v)
{
    output[0] = v & 0xff;
    output[1] = (v >> 8) & 0xff;
    output[2] = (v >> 16) & 0xff;
    output[3] = (v >> 24) & 0xff;
    output[4] = (v >> 32) & 0xff;
    output[5] = (v >> 40) & 0xff;
    output[6] = (v >> 48) & 0xff;
    output[7] = (v >> 56) & 0xff;
}

void copy_le32(uint8_t *output, uint32_t v)
{
    output[0] = v & 0xff;
    output[1] = (v >> 8) & 0xff;
    output[2] = (v >> 16) & 0xff;
    output[3] = (v >> 24) & 0xff;
}

void copy_le16(uint8_t *output, uint16_t v)
{
    output[0] = v & 0xff;
    output[1] = (v >> 8) & 0xff;
}

#ifndef FWUP_APPLY_ONLY
// Random numbers are only needed for public/private key pair creation.  Since
// cryptographic random number generation requires scrutiny to ensure
// correctness, do not include these on minimal builds so that there's no need
// to verify correctness on devices that use them.

#if defined(__linux__)
#if HAVE_SYS_RANDOM_H
#include <sys/random.h>

int get_random(uint8_t *buf, size_t len)
{
   return getrandom(buf, len, 0);
}
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int get_random(uint8_t *buf, size_t len)
{
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return -1;

    ssize_t amt = read(fd, buf, len);
    close(fd);

    if (amt == ((size_t) len))
        return 0;
    else
        return -1;
}
#endif

#endif

#if defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__) || \
    defined(__APPLE__)
int get_random(uint8_t *buf, size_t len)
{
    arc4random_buf(buf, len);
    return 0;
}
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>

#ifdef THIS_FAILS_ON_WINE_AND_I_DONT_KNOW_WHY
#include <bcrypt.h>
int get_random(uint8_t *buf, size_t len)
{
    BCRYPT_ALG_HANDLE handle;

    if (BCryptOpenAlgorithmProvider(&handle, BCRYPT_RNG_ALGORITHM, NULL, 0) != 0 &&
        BCryptGenRandom(handle, buf, len, 0) != 0) {
        BCryptCloseAlgorithmProvider(handle, 0);
        return 0;
    } else {
        return -1;
    }
}
#else
#include <ntsecapi.h>
int get_random(uint8_t *buf, size_t len)
{
    RtlGenRandom(buf, len);
    return 0;
}
#endif
#endif // _WIN32 || __CYGWIN__
#endif // FWUP_APPLY_ONLY
