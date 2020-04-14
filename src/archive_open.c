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

#include "archive_open.h"
#include "util.h"

#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DEFAULT_LIBARCHIVE_BLOCK_SIZE 16384

struct fwup_archive_data {
    size_t current_frame_remaining;
    bool is_stdin;
    bool is_eof;
    int fd;

    char name[PATH_MAX];
    char buffer[DEFAULT_LIBARCHIVE_BLOCK_SIZE];
};

static ssize_t normal_read(struct archive *a, void *client_data, const void **buff)
{
    struct fwup_archive_data *ad = (struct fwup_archive_data *) client_data;

    *buff = ad->buffer;
    for (;;) {
        ssize_t bytes_read = read(ad->fd, ad->buffer, sizeof(ad->buffer));
        if (bytes_read < 0) {
            if (errno == EINTR)
                continue;

            if (ad->is_stdin)
                archive_set_error(a, errno, "Error reading stdin");
            else
                archive_set_error(a, errno, "Error reading '%s'", ad->name);

            return -1;
        }

        return bytes_read;
    }
}

static int normal_close(struct archive *a, void *client_data)
{
    struct fwup_archive_data *ad = (struct fwup_archive_data *) client_data;
    (void)a; /* UNUSED */

    // NOTE: One very important difference between libarchive's default file
    // reading implementation and this one is that libarchive drains stdin
    // before closing it. This is the friendly thing to do when working with
    // pipes since if data isn't needed at the end, you still have to consume
    // it or the source of the pipe gets an error. This is NOT the behavior
    // we want. We want to leave it up to other code for whether to drain or
    // not. That way if there's an error, that code can choose to exit immediately
    // and not pay the cost of transferring all of those bytes through the pipe.
    // The pipe data may originate from a remote source and stopping the
    // transmission immediately saves time.

    // Only close files - not stdin.
    if (ad->fd > 0)
        close(ad->fd);

    free(ad);
    return ARCHIVE_OK;
}

static ssize_t framed_stdin_read(struct archive *a, void *client_data, const void **buff)
{
    struct fwup_archive_data *ad = (struct fwup_archive_data *) client_data;
    if (ad->is_eof)
        return 0;

    *buff = ad->buffer;

    size_t amount_read;

    if (ad->current_frame_remaining == 0) {
        uint32_t be_len;
        amount_read = fread(&be_len, 1, sizeof(be_len), stdin);
        if (amount_read != sizeof(be_len)) {
            archive_set_error(a, errno, "Error reading stdin");
            return -1;
        }

        if (be_len == 0) {
            ad->is_eof = true;
            return 0;
        }
        ad->current_frame_remaining = FROM_BIGENDIAN32(be_len);
    }

    size_t amount_to_read = ad->current_frame_remaining;
    if (amount_to_read > sizeof(ad->buffer))
        amount_to_read = sizeof(ad->buffer);

    amount_read = fread(ad->buffer, 1, amount_to_read, stdin);
    if (amount_read == 0) {
        archive_set_error(a, EIO, "Received EOF even though framing indicated more bytes");
        return -1;
    } else {
        ad->current_frame_remaining -= amount_read;
        return amount_read;
    }
}

/**
 * @brief Open the specified file for use with libarchive.
 *
 * This call sets up all of the libarchive callbacks properly for fwup.
 *
 * @param a a libarchive handle
 * @param filename the file to open or NULL for stdin
 * @return a libarchive error code (e.g., ARCHIVE_OK or ARCHIVE_FATAL)
 */
int fwup_archive_open_filename(struct archive *a, const char *filename)
{
    struct fwup_archive_data *ad = (struct fwup_archive_data *) calloc(1, sizeof(struct fwup_archive_data));
    if (ad == NULL) {
        archive_set_error(a, ENOMEM, "No memory");
        return ARCHIVE_FATAL;
    }

    ad->current_frame_remaining = 0;
    ad->is_eof = false;
    ad->is_stdin = (filename == NULL || filename[0] == '\0');
    if (!ad->is_stdin)
        strncpy(ad->name, filename, sizeof(ad->name) - 1);

    if (ad->is_stdin) {
        ad->fd = STDIN_FILENO;
#ifdef _WIN32
        setmode(STDIN_FILENO, O_BINARY);
#endif
    } else {
        ad->fd = open(ad->name, O_RDONLY | O_WIN32_BINARY);
        if (ad->fd < 0) {
            archive_set_error(a, errno, "Failed to open '%s'", ad->name);
            return ARCHIVE_FATAL;
        }
#ifdef HAVE_FCNTL
        (void) fcntl(ad->fd, F_SETFD, FD_CLOEXEC);
#endif
    }

    archive_read_set_callback_data(a, ad);
    archive_read_set_close_callback(a, normal_close);

    if (fwup_framing && ad->is_stdin) {
        // If reading from standard in and framing is enabled, then
        // it needs to be applied to the input too.
        archive_read_set_read_callback(a, framed_stdin_read);
    } else {
        // Files and stdin w/o framing are handled similarly.
        archive_read_set_read_callback(a, normal_read);
    }

    return archive_read_open1(a);
}
