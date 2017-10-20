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

#ifndef MBR_H
#define MBR_H

#include <stdbool.h>
#include <stdint.h>
#include <confuse.h>

struct mbr_partition {
    bool boot_flag;     // true to mark as boot partition
    int partition_type; // partition type (e.g., 0=unused, 0x83=Linux, 0x01=FAT12, 0x04=FAT16, 0x0c=FAT32, etc.
    uint32_t block_offset;
    uint32_t block_count;
};

struct osii {
    uint16_t os_minor;
    uint16_t os_major;
    uint32_t start_block_offset; // units of block size of media (512 bytes for eMMC)
    uint32_t ddr_load_address;
    uint32_t entry_point;
    uint32_t image_size;   // units of block size
    uint8_t attribute;
    uint8_t reserved[3];
};

struct osip_header {
    bool include_osip;

    uint8_t minor;
    uint8_t major;
    uint8_t num_pointers;
    uint8_t num_images;

    struct osii descriptors[16];
};

int mbr_verify_cfg(cfg_t *cfg);
int mbr_create_cfg(cfg_t *cfg, uint8_t output[512]);

int mbr_create(const struct mbr_partition partitions[4],
               const uint8_t *bootstrap,
               const struct osip_header *osip,
               uint32_t signature,
               uint8_t output[512]);
int mbr_verify(const struct mbr_partition partitions[4]);
int mbr_decode(const uint8_t input[512], struct mbr_partition partitions[4]);

#endif // MBR_H
