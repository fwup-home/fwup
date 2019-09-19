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

#ifndef GPT_H
#define GPT_H

#include <stdbool.h>
#include <stdint.h>
#include <confuse.h>

#include "util.h"

// fwup only supports 16 partitions even though GPT supports more.
// There's nothing fundemental limiting this. The partition info
// is stored on the stack.
#define GPT_MAX_PARTITIONS 16

#define GPT_PARTITION_TABLE_BLOCKS 32
#define GPT_SIZE_BLOCKS (1 + GPT_PARTITION_TABLE_BLOCKS)
#define GPT_SIZE (GPT_SIZE_BLOCKS * FWUP_BLOCK_SIZE)

int gpt_verify_cfg(cfg_t *cfg);
int gpt_create_cfg(cfg_t *cfg, uint32_t num_blocks, uint8_t *primary_gpt, uint8_t *secondary_gpt, off_t *secondary_gpt_offset);

#endif // GPT_H
