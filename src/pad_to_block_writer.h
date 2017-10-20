/*
 * Copyright 2017 Frank Hunleth
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

#ifndef PAD_TO_BLOCK_WRITER_H
#define PAD_TO_BLOCK_WRITER_H

#include "util.h"

struct block_cache;

struct pad_to_block_writer
{
    struct block_cache *output;

    uint8_t buffer[FWUP_BLOCK_SIZE];
    size_t index;
    off_t offset;
};
void ptbw_init(struct pad_to_block_writer *ptbw, struct block_cache *output);
int ptbw_pwrite(struct pad_to_block_writer *ptbw, const void *buf, size_t count, off_t offset);
int ptbw_flush(struct pad_to_block_writer *ptbw);

#endif // PAD_TO_BLOCK_WRITER_H
