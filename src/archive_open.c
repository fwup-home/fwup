/*
 * Copyright 2016 Frank Hunleth
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

#ifdef _WIN32
#include <fcntl.h>
#endif

#define DEFAULT_LIBARCHIVE_BLOCK_SIZE 16384

extern bool fwup_framing;

struct fwup_archive_data {
    size_t current_frame_remaining;
    bool is_eof;
    char buffer[4096];
};

static int framed_stdin_open(struct archive *a, void *client_data)
{
    struct fwup_archive_data *ad = (struct fwup_archive_data *) client_data;

#ifdef _WIN32
    setmode(0, O_BINARY);
#endif

    ad->current_frame_remaining = 0;

    archive_clear_error(a);
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

static int framed_stdin_close(struct archive *a, void *client_data)
{
    (void) a; // UNUSED

    free(client_data);
    return ARCHIVE_OK;
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
    // If framing isn't enabled or we're reading from a file, then
    // the built-in libarchive reading functions are fine.
    if (!fwup_framing ||
            (filename != NULL && filename[0] != '\0'))
        return archive_read_open_filename(a, filename, DEFAULT_LIBARCHIVE_BLOCK_SIZE);

    // Reading from stdin with framing
    struct fwup_archive_data *ad = (struct fwup_archive_data *) malloc(sizeof(struct fwup_archive_data));
    if (ad == NULL) {
        archive_set_error(a, ENOMEM, "No memory");
        return ARCHIVE_FATAL;
    }

    return archive_read_open(a, ad, framed_stdin_open, framed_stdin_read, framed_stdin_close);
}
