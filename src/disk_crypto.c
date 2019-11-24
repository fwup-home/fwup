/*
 * Copyright 2019 Frank Hunleth
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

#include "disk_crypto.h"
#include "3rdparty/tiny-AES-c/aes.h"
#include "3rdparty/base64.h"
#include "monocypher.h"

#include <string.h>

static void aes_cbc_plain_encrypt(struct disk_crypto *dc, uint32_t lba, const uint8_t *input, uint8_t *output)
{
    uint8_t iv[AES_BLOCKLEN] = {0};
    copy_le32(iv, lba);

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, dc->key, iv);

    // Tiny AES only encrypts in-place, so copy to the output if necessary.
    if (output != input)
        memcpy(output, input, FWUP_BLOCK_SIZE);

    AES_CBC_encrypt_buffer(&ctx, output, FWUP_BLOCK_SIZE);
}
static int aes_cbc_plain_init(struct disk_crypto *dc, const char *secret_key)
{
    dc->encrypt = aes_cbc_plain_encrypt;

    if (secret_key) {
        if (hex_to_bytes(secret_key, dc->key, AES_KEYLEN) == 0)
            return 0;

        // Try base64 since that was was used in fwup 1.5.0, but it turned out
        // to be inconvenient to actually use. Do not copy/paste this to other
        // cipher options.
        size_t decoded_len = AES_KEYLEN;
        if (from_base64(dc->key, &decoded_len, secret_key) != NULL &&
            decoded_len == AES_KEYLEN)
            return 0;
    }

    ERR_RETURN("aes-cbc-plain requires a hex-encoded %d-bit key", AES_KEYLEN * 8);
}

/**
 * Initialize a disk crypto session
 *
 * @param dc session info
 * @param cipher which cipher (e.g., "aes-cbc-plain")
 * @param secret the secret key (base64 encoded)
 * @param base_offset subtract this offset from every block being written
 * @return 0 on success
 */
int disk_crypto_init(struct disk_crypto *dc, const char *cipher, const char *secret, off_t base_offset)
{
    dc->base_offset = base_offset;

    if (strcmp(cipher, "aes-cbc-plain") == 0)
        return aes_cbc_plain_init(dc, secret);
    else
        ERR_RETURN("Unsupported disk encryption: %s", cipher);
}

/**
 * @brief disk_crypto_encrypt
 * @param dc session info
 * @param input
 * @param output
 * @param count the number of bytes to write
 * @param offset where the bytes will be written
 */
void disk_crypto_encrypt(struct disk_crypto *dc, const uint8_t *input, uint8_t *output, size_t count, off_t offset)
{
    uint32_t lba = (uint32_t) ((offset - dc->base_offset)/ FWUP_BLOCK_SIZE);
    uint32_t last_lba = lba + (uint32_t) (count / FWUP_BLOCK_SIZE);

    while (lba < last_lba) {
        dc->encrypt(dc, lba, input, output);
        lba++;
        input += FWUP_BLOCK_SIZE;
        output += FWUP_BLOCK_SIZE;
    }
}

/**
 * Free resources associated with a disk crypto session
 *
 * @param dc session info
 */
void disk_crypto_free(struct disk_crypto *dc)
{
    memset(dc, 0, sizeof(struct disk_crypto));
}
