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

#include "fwup_genkeys.h"
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "../3rdparty/base64.h"
#include "util.h"

static int save_key(const char *name, unsigned char *key, size_t key_len)
{
    int rc = 0;

    // O_EXCL -> make sure that file doesn't already exist when calling
    //           open() so that we don't accidentally overwrite a file.
    int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        ERR_RETURN("Couldn't create '%s': %s", name, strerror(errno));

    size_t encoded_len = base64_raw_to_encoded_count(key_len);
    char buffer[encoded_len + 1];
    size_t unpadded_len = to_base64(buffer, encoded_len, key, key_len);

    // The libsodium base64 code doesn't pad. This isn't a problem for
    // fwup, but triggers decode errors when the output is run through
    // base64. See https://tools.ietf.org/html/rfc4648#page-4.
    while (unpadded_len < encoded_len)
        buffer[unpadded_len++] = '=';

    ssize_t written = write(fd, buffer, encoded_len);
    if (written < 0 || (size_t) written != encoded_len)
        ERR_CLEANUP_MSG("Couldn't write to '%s': %s", name, strerror(errno));

cleanup:
    close(fd);
    return rc;
}

int fwup_genkeys()
{
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    int rc;

    if (crypto_sign_keypair(pk, sk) < 0)
        ERR_RETURN("Error creating key pair");

    OK_OR_CLEANUP(save_key("fwup-key.pub", pk, sizeof(pk)));
    OK_OR_CLEANUP(save_key("fwup-key.priv", sk, sizeof(sk)));

    fwup_output(FRAMING_TYPE_SUCCESS, 0,
        "Firmware signing keys created and saved to fwup-key.pub and fwup-key.priv\n\n"
        "Distribute fwup-key.pub with your system so that firmware updates can be\n"
        "authenticated. Keep fwup-key.priv in a safe location.\n");

    return 0;

cleanup:
    unlink("fwup-key.pub");
    unlink("fwup-key.priv");
    return rc;
}
