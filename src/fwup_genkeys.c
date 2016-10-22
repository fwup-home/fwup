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

#include "fwup_genkeys.h"
#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

static int save_key(const char *name, unsigned char *c, size_t len)
{
    FILE *fp = fopen(name, "wb");
    if (!fp)
        ERR_RETURN("Couldn't create '%s'", name);

    if (fwrite(c, 1, len, fp) != len) {
        fclose(fp);
        ERR_RETURN("Couldn't write to '%s'", name);
    }

    fclose(fp);
    return 0;
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

    printf("Firmware signing keys created and saved to fwup-key.pub and fwup-key.priv\n\n");
    printf("Distribute fwup-key.pub with your system so that firmware updates can be\n");
    printf("authenticated. Keep fwup-key.priv in a safe location.\n");

    return 0;

cleanup:
    unlink("fwup-key.pub");
    unlink("fwup-key.priv");
    return rc;
}
