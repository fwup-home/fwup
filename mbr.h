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

#ifndef MBR_H
#define MBR_H

#include <stdbool.h>
#include <stdint.h>

struct mbr_partition {
    bool boot_flag;     // true to mark as boot partition
    int partition_type; // partition type (e.g., 0=unused, 0x83=Linux, 0x01=FAT12, 0x04=FAT16, 0x0c=FAT32, etc.
    int block_offset;
    int block_count;
};

int mbr_create(const struct mbr_partition partitions[4], const uint8_t *bootstrap, uint8_t output[512]);
int mbr_verify(const struct mbr_partition partitions[4]);
int mbr_decode(const uint8_t input[512], struct mbr_partition partitions[4]);

const char *mbr_last_error();

#endif // MBR_H
