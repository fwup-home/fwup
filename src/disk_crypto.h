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

#ifndef DISK_CRYPTO_H
#define DISK_CRYPTO_H

#include "util.h"

struct disk_crypto;
typedef void (disk_crypto_fun)(struct disk_crypto *dc, uint32_t lba, const uint8_t *input, uint8_t *output);

struct disk_crypto {
    disk_crypto_fun *encrypt;
    disk_crypto_fun *decrypt;
    uint8_t key[32];
    off_t base_offset;
};

int disk_crypto_init(struct disk_crypto *dc, off_t base_offset, int argc, const char *argv[]);
void disk_crypto_encrypt(struct disk_crypto *dc, const uint8_t *input, uint8_t *output, size_t count, off_t offset);
void disk_crypto_decrypt(struct disk_crypto *dc, const uint8_t *input, uint8_t *output, size_t count, off_t offset);
void disk_crypto_free(struct disk_crypto *dc);

#endif // EVAL_MATH_H


