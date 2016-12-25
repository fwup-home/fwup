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
#define _GNU_SOURCE // for vasprintf
#include "util.h"
#include "simple_string.h"
#include "progress.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>

char *strptime(const char *s, const char *format, struct tm *tm);
extern bool fwup_framing;
extern enum fwup_progress_option fwup_progress_mode;

static char *last_error_message = NULL;
static char time_string[200] = {0};

static const char *timestamp_format = "%Y-%m-%dT%H:%M:%SZ";

const char *get_creation_timestamp()
{
    // Ensure that if the creation timestamp is queried more than
    // once that the same string gets returned.
    if (*time_string == '\0') {
        const char *now = get_environment("NOW");
        if (now != NULL) {
            // The user specified NOW, so check that it's parsable.
            struct tm tmp;
            if (strptime(now, timestamp_format, &tmp) != NULL) {
                int rc = snprintf(time_string, sizeof(time_string), "%s", now);
                if (rc >= 0 && rc < (int) sizeof(time_string))
                    return time_string;
            }

            INFO("NOW environment variable set, but not in YYYY-MM-DDTHH:MM:SSZ format so ignoring");
        }
        time_t t = time(NULL);
        struct tm *tm_now = gmtime(&t);
        if (tm_now == NULL)
            fwup_err(EXIT_FAILURE, "gmtime");

        strftime(time_string, sizeof(time_string), timestamp_format, tm_now);
        set_environment("NOW", time_string);
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

int hex_to_bytes(const char *str, uint8_t *bytes, size_t maxbytes)
{
    size_t len = strlen(str);
    if (len & 1)
        ERR_RETURN("hex string should have an even number of characters");

    if (len / 2 > maxbytes)
        ERR_RETURN("hex string is too long (%d bytes)", len / 2);

    while (len) {
        uint8_t sixteens = hexchar_to_int(str[0]);
        uint8_t ones = hexchar_to_int(str[1]);
        if (sixteens == 255 || ones == 255)
            ERR_RETURN("Invalid character in hex string");

        *bytes = (sixteens << 4) | ones;
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

/*
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
    // See https://github.com/fhunleth/fwup/issues/35.

    if (strncmp(path, "/dev/", 5) == 0)
        is_in_dev = true;
#endif

#ifdef _WIN32
    if (strncmp(path, "\\\\.\\", 4) == 0)
        return 0;
#endif
    struct stat st;
    int rc = stat(path, &st);
    return (rc == 0 && (st.st_mode & S_IFREG)) || // Existing regular file
           (rc < 0 && errno == ENOENT && !is_in_dev); // Doesn't exist and not in /dev
}

/*
 * Return true if the file exists.
 */
bool file_exists(const char *path)
{
    struct stat st;
    int rc = stat(path, &st);
    return rc == 0;
}


void format_pretty_size(off_t amount, char *out, size_t out_size)
{
    if (amount >= ONE_GiB)
        snprintf(out, out_size, "%.2f GiB", ((double) amount) / ONE_GiB);
    else if (amount >= ONE_MiB)
        snprintf(out, out_size, "%.2f MiB", ((double) amount) / ONE_MiB);
    else if (amount >= ONE_KiB)
        snprintf(out, out_size, "%d KiB", (int) (amount / ONE_KiB));
    else
        snprintf(out, out_size, "%d bytes", (int) amount);
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
    exit(status);

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
    exit(status);

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
    } else if (fwup_progress_mode == PROGRESS_MODE_NORMAL) {
        // Erase the current % and then print the message
        fwrite("\r   \r", 5, 1, stdout);
    }
    fwrite(str, 1, len, stdout);
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
