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

#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include "util.h"

#ifndef FWUP_MINIMAL

static int save_key(const char *name, unsigned char *key, size_t key_len)
{
    int rc = 0;

    // O_EXCL -> make sure that file doesn't already exist when calling
    //           open() so that we don't accidentally overwrite a file.
    int fd = open(name, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        ERR_RETURN("Couldn't create '%s': %s", name, strerror(errno));

    size_t buffer_len = sodium_base64_encoded_len(key_len, sodium_base64_VARIANT_ORIGINAL);
    char buffer[buffer_len];
    sodium_bin2base64(buffer, buffer_len, key, key_len, sodium_base64_VARIANT_ORIGINAL);

    size_t encoded_len = buffer_len - 1;
    ssize_t written = write(fd, buffer, encoded_len);
    if (written < 0 || (size_t) written != encoded_len)
        ERR_CLEANUP_MSG("Couldn't write to '%s': %s", name, strerror(errno));

cleanup:
    close(fd);
    return rc;
}

/**
 * @brief Generate a public/private signing key pair
 * @param output_prefix if non-NULL, this is the prefix to the output file
 * @return 0 on success
 */
int fwup_genkeys(const char *output_prefix)
{
    char pubkey_path[PATH_MAX];
    char privkey_path[PATH_MAX];
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];

    if (crypto_sign_keypair(pk, sk) < 0)
        ERR_RETURN("Error creating key pair");

    if (!output_prefix)
        output_prefix = "fwup-key";

    sprintf(pubkey_path, "%s.pub", output_prefix);
    sprintf(privkey_path, "%s.priv", output_prefix);

    OK_OR_RETURN(save_key(pubkey_path, pk, sizeof(pk)));
    OK_OR_RETURN(save_key(privkey_path, sk, sizeof(sk)));

    char message[512];
    char *base_pubkey_path = basename(pubkey_path);
    char *base_privkey_path = basename(privkey_path);
    sprintf(message, "Firmware signing keys created and saved to '%.32s' and '%.32s'\n\n"
                     "Distribute '%.32s' with your system so that firmware updates can be\n"
                     "authenticated. Keep '%.32s' in a safe location.\n",
            base_pubkey_path, base_privkey_path, base_pubkey_path, base_privkey_path);

    fwup_output(FRAMING_TYPE_SUCCESS, 0, message);

    return 0;
}

#endif // FWUP_MINIMAL
