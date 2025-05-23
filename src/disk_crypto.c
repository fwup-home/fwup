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

#define MAX_CIPHER_LEN 32
#define MAX_SECRET_LEN 64

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
static void aes_cbc_plain_decrypt(struct disk_crypto *dc, uint32_t lba, const uint8_t *input, uint8_t *output)
{
    uint8_t iv[AES_BLOCKLEN] = {0};
    copy_le32(iv, lba);

    struct AES_ctx ctx;
    AES_init_ctx_iv(&ctx, dc->key, iv);

    // Tiny AES only decrypts in-place, so copy to the output if necessary.
    if (output != input)
        memcpy(output, input, FWUP_BLOCK_SIZE);

    AES_CBC_decrypt_buffer(&ctx, output, FWUP_BLOCK_SIZE);
}
static int aes_cbc_plain_init(struct disk_crypto *dc, const char *secret_key)
{
    dc->encrypt = aes_cbc_plain_encrypt;
    dc->decrypt = aes_cbc_plain_decrypt;

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

static int min(int a, int b)
{
    return (a < b) ? a : b;
}

static int parse_options(int argc, const char *argv[], char *cipher, char *secret)
{
    memset(cipher, 0, MAX_CIPHER_LEN + 1);
    memset(secret, 0, MAX_SECRET_LEN + 1);

    for (int i = 0; i < argc; i++) {
        const char *key;
        const char *value;

        key = argv[i];
        do {
            value = strchr(key, '=');
            if (!value)
                ERR_RETURN("Expecting '=' for in parameter '%s'", key);

            value++;

            const char *next_key = strchr(value, ',');
            size_t value_len;
            if (next_key) {
                value_len = (size_t) (next_key - value);
                next_key++;
            } else {
                value_len = strlen(value);
            }

            if (strncmp(key, "cipher=", 7) == 0)
                strncpy(cipher, value, min(MAX_CIPHER_LEN, value_len));
            else if (strncmp(key, "secret=", 7) == 0)
                strncpy(secret, value, min(MAX_SECRET_LEN, value_len));
            else
                ERR_RETURN("Unexpected parameter: %s", key);

            key = next_key;
        } while (key);
    }

    // If there's a cipher, then there must be a secret
    if ((*cipher && !*secret) ||
        (*secret && !*cipher))
        ERR_RETURN("Both a cipher and secret are required if one is supplied");

    return 0;
}

/**
 * Initialize a disk crypto session
 *
 * @param dc session info
 * @param argc argument count  which cipher (e.g., "aes-cbc-plain")
 * @param argv arguments (e.g. "cipher=aes-cbc-plain,secret=<base64 encoded>")
 * @param base_offset subtract this offset from every block being written
 * @return 0 on success
 */
int disk_crypto_init(struct disk_crypto *dc, off_t base_offset, int argc, const char *argv[])
{
    char cipher[MAX_CIPHER_LEN + 1];
    char secret[MAX_SECRET_LEN + 1];

    OK_OR_RETURN(parse_options(argc, argv, cipher, secret));

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
 * @param count the number of bytes to write. Must be a multiple of 512.
 * @param offset where the bytes will be written. Must be a multiple of 512.
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
 * @brief disk_crypto_decrypt
 * @param dc session info
 * @param input
 * @param output
 * @param count the number of bytes to write. Must be a multiple of 512.
 * @param offset where the bytes will be written. Must be a multiple of 512.
 */
void disk_crypto_decrypt(struct disk_crypto *dc, const uint8_t *input, uint8_t *output, size_t count, off_t offset)
{
    uint32_t lba = (uint32_t) ((offset - dc->base_offset)/ FWUP_BLOCK_SIZE);
    uint32_t last_lba = lba + (uint32_t) (count / FWUP_BLOCK_SIZE);

    while (lba < last_lba) {
        dc->decrypt(dc, lba, input, output);
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
