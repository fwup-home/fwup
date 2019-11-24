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

#include "gpt.h"
#include "crc32.h"
#include "util.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GPT_PARTITION_SIZE 128

struct gpt_partition {
    uint32_t block_offset;
    uint32_t block_count;
    uint64_t flags;     // See spec for meaning of bits
    uint8_t partition_type[UUID_LENGTH];
    uint8_t guid[UUID_LENGTH];
    char name[72];      // Encoded as UTF-16LE
    bool expand_flag;   // true to indicate that fwup can grow this partition
    bool valid;         // true if valid partition
    uint8_t padding[6];
};

struct gpt_header {
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[UUID_LENGTH];
    uint64_t partition_lba;

    int num_partitions;
    uint32_t partition_crc;
};

/**
 * @brief gpt_verify check that the specified partitions make sense and don't overlap
 * @param partitions the partitions
 * @return 0 if successful
 */
static int gpt_verify_partitions(const struct gpt_partition partitions[GPT_MAX_PARTITIONS])
{
    bool expanding = false;
    int i;

    // Check for overlap
    for (i = 0; i < GPT_MAX_PARTITIONS; i++) {
        uint32_t ileft = partitions[i].block_offset;
        uint32_t iright = ileft + partitions[i].block_count;

        // Check if unused.
        if (!partitions[i].valid)
           continue;

        if (ileft == iright && !partitions[i].expand_flag)
            continue;

        // Validate that if expand is used, it has to be the last partition
        if (expanding)
            ERR_RETURN("a partition can't be specified after the one with \"expand = true\"");

        if (partitions[i].expand_flag)
            expanding = true;

        int j;
        for (j = 0; j < GPT_MAX_PARTITIONS; j++) {
            if (!partitions[j].valid || j == i)
                continue;

            uint32_t jleft = partitions[j].block_offset;
            uint32_t jright = jleft + partitions[j].block_count;

            if ((ileft >= jleft && ileft < jright) ||
                (iright > jleft && iright <= jright))
                ERR_RETURN("partitions %d (blocks %u to %u) and %d (blocks %u to %u) overlap",
                        i, ileft, iright, j, jleft, jright);
        }
    }

    return 0;
}

static void create_protective_mbr(uint8_t *output, uint32_t num_blocks)
{
    // First partition covers the entire disk or as much as possible
    output[446] = 0; // Boot flag
    output[446 + 2] = 0x02; // Match output of sfdisk?
    output[446 + 4] = 0xee; // Protective MBR partition
    output[446 + 5] = 0xff; // "End CHS" = 0xffffff
    output[446 + 6] = 0xff;
    output[446 + 7] = 0xff;
    copy_le32(&output[446 + 8], 1);
    copy_le32(&output[446 + 12], num_blocks - 1);

    // MBR signature
    output[510] = 0x55;
    output[511] = 0xaa;
}

static void create_partition(const struct gpt_partition *partition, uint8_t *output, uint32_t num_blocks)
{
    uint32_t block_count = partition->block_count;

    // If expanding and we know the total blocks, update this partition to the max
    if (partition->expand_flag &&
        num_blocks > (partition->block_offset + partition->block_count + GPT_SIZE_BLOCKS))
        block_count = num_blocks - GPT_SIZE_BLOCKS - 1 - partition->block_offset;

    uint64_t first_lba = partition->block_offset;
    uint64_t last_lba = partition->block_offset + block_count - 1;

    memcpy(&output[0], partition->partition_type, UUID_LENGTH);
    memcpy(&output[16], partition->guid, UUID_LENGTH);
    copy_le64(&output[32], first_lba);
    copy_le64(&output[40], last_lba);
    copy_le64(&output[48], partition->flags);
    memcpy(&output[56], partition->name, 72);
}

static void create_partitions(const struct gpt_partition *partitions, uint32_t num_blocks, uint8_t *output)
{
    for (int i = 0; i < GPT_MAX_PARTITIONS; i++) {
        if (partitions[i].valid)
            create_partition(&partitions[i], output, num_blocks);

        output += GPT_PARTITION_SIZE;
    }
}

static void create_gpt_header(const struct gpt_header *header,
                             uint8_t *output)
{
    const size_t header_size = 92;

    // Zero out the block per the spec (it should already be zero)
    memset(output, 0, FWUP_BLOCK_SIZE);

    // Signature
    memcpy(&output[0], "EFI PART", 8);

    // Revision (GPT 1.0)
    copy_le32(&output[8], 0x00010000);

    // Header size (92 bytes)
    copy_le32(&output[12], header_size);

    // CRC32 pre-calculation (offset 16)

    // Reserved (offset 20)

    // Header locations
    copy_le64(&output[24], header->current_lba);
    copy_le64(&output[32], header->backup_lba);

    // Usable locations
    copy_le64(&output[40], header->first_usable_lba);
    copy_le64(&output[48], header->last_usable_lba);

    // Disk GUID
    memcpy(&output[56], header->disk_guid, UUID_LENGTH);

    // Partition table location
    copy_le64(&output[72], header->partition_lba);

    // Number of partition entries
    copy_le32(&output[80], header->num_partitions);

    // Partition entry size
    copy_le32(&output[84], GPT_PARTITION_SIZE);

    copy_le32(&output[88], header->partition_crc);

    // Final header CRC32
    uint32_t header_crc = crc32buf((const char *) output, header_size);
    copy_le32(&output[16], header_crc);
}

static int gpt_cfg_to_partitions(cfg_t *cfg, struct gpt_partition *partitions, int *found_partitions)
{
    cfg_t *partition;
    int i = 0;
    int found = 0;

    memset(partitions, 0, GPT_MAX_PARTITIONS * sizeof(struct gpt_partition));

    while ((partition = cfg_getnsec(cfg, "partition", i++)) != NULL) {
        unsigned long partition_ix = strtoul(cfg_title(partition), NULL, 0);
        if (partition_ix >= GPT_MAX_PARTITIONS)
            ERR_RETURN("partition must be numbered 0 through %d", GPT_MAX_PARTITIONS - 1);

        if (found & (1 << partition_ix))
            ERR_RETURN("invalid or duplicate partition number found for %d", partition_ix);
        found = found | (1 << partition_ix);

        const char *unverified_type = cfg_getstr(partition, "type");
        if (!unverified_type || string_to_uuid_me(unverified_type, partitions[partition_ix].partition_type) < 0)
            ERR_RETURN("partition %d's type must set to a UUID", partition_ix);

        const char *unverified_guid = cfg_getstr(partition, "guid");
        if (!unverified_guid || string_to_uuid_me(unverified_guid, partitions[partition_ix].guid) < 0)
            ERR_RETURN("partition %d must have a valid guid", partition_ix);

        const char *unverified_name = cfg_getstr(partition, "name");
        size_t name_len = strlen(unverified_name);
        if (name_len > 36)
            name_len = 36;
        ascii_to_utf16le(unverified_name, partitions[partition_ix].name, name_len);

        const char *unverified_block_offset = cfg_getstr(partition, "block-offset");
        if (!unverified_block_offset || *unverified_block_offset == '\0')
            ERR_RETURN("partition %d's block_offset is required", partition_ix);
        char *endptr;
        unsigned long block_offset = strtoul(unverified_block_offset, &endptr, 0);

        const char *unverified_flags = cfg_getstr(partition, "flags");
        if (!unverified_flags)
            ERR_RETURN("partition %d's flags is required", partition_ix);
        uint64_t flags = strtoull(unverified_flags, &endptr, 0);

        // strtoul returns error by returning ULONG_MAX and setting errno.
        // Values bigger than 2^32-1 won't fit in the MBR, so report an
        // error for those too.
        if ((block_offset == ULONG_MAX && errno != 0) || block_offset >= UINT32_MAX)
            ERR_RETURN("partition %d's block_offset must be positive and less than 2^32 - 1: '%s'", partition_ix, unverified_block_offset);
        if (*endptr != '\0')
            ERR_RETURN("error parsing partition %d's block offset", partition_ix);

        partitions[partition_ix].block_offset = block_offset;

        partitions[partition_ix].block_count = cfg_getint(partition, "block-count");
        if (partitions[partition_ix].block_count >= INT32_MAX)
            ERR_RETURN("partition %d's block-count must be specified and less than 2^31 - 1", partition_ix);

        partitions[partition_ix].expand_flag = cfg_getbool(partition, "expand");
        partitions[partition_ix].flags = flags;
        partitions[partition_ix].valid = true;
    }

    if (found_partitions)
        *found_partitions = found;
    return 0;
}

int gpt_verify_cfg(cfg_t *cfg)
{
    uint8_t guid[UUID_LENGTH];
    const char *unverified_guid = cfg_getstr(cfg, "guid");
    if (!unverified_guid || string_to_uuid_me(unverified_guid, guid) < 0)
        ERR_RETURN("GPT must have a valid disk guid");

    int found_partitions = 0;
    struct gpt_partition partitions[GPT_MAX_PARTITIONS];

    if (gpt_cfg_to_partitions(cfg, partitions, &found_partitions) < 0)
        return -1;
    if (found_partitions == 0)
        ERR_RETURN("empty partition table?");

    return gpt_verify_partitions(partitions);
}

static uint32_t compute_num_blocks(const struct gpt_partition *partitions)
{
    uint32_t num_blocks = 0;
    for (int i = 0; i < GPT_MAX_PARTITIONS; i++) {
        if (partitions[i].valid) {
            uint32_t last_block = partitions[i].block_offset + partitions[i].block_count;
            if (last_block > num_blocks)
                num_blocks = last_block;
        }
    }

    // Add room for secondary GPT
    num_blocks += GPT_SIZE_BLOCKS + 1;

    return num_blocks;
}

/**
 * @brief Encode the GPT headers
 *
 * After calling this function, the primary_gpt should be written starting at LBA 0 and
 * the secondary_gpt should be written at byte offset secondary_gpt_offset.
 *
 * @param cfg the mbr configuration
 * @param num_blocks the number of blocks on the destination or 0 if unknown
 * @param mbr_and_primary_gpt where to store the MBR and primary GPT (must be GPT_SIZE + 512 bytes)
 * @param secondary_gpt where to store the secondary GPT (must be GPT_SIZE bytes)
 * @param secondary_gpt_offset where to write the secondary GPT on disk
 * @return 0 if successful
 */
int gpt_create_cfg(cfg_t *cfg, uint32_t num_blocks, uint8_t *mbr_and_primary_gpt, uint8_t *secondary_gpt, off_t *secondary_gpt_offset)
{
    struct gpt_header header;
    struct gpt_partition partitions[GPT_MAX_PARTITIONS];

    if (gpt_cfg_to_partitions(cfg, partitions, NULL) < 0)
        return -1;

    if (num_blocks == 0)
        num_blocks = compute_num_blocks(partitions);

    // Clear everything out.
    memset(mbr_and_primary_gpt, 0, GPT_SIZE + FWUP_BLOCK_SIZE);
    memset(secondary_gpt, 0, GPT_SIZE);

    // Create the protective MBR
    create_protective_mbr(mbr_and_primary_gpt, num_blocks);

    // Create the partition table entries
    create_partitions(partitions, num_blocks, &mbr_and_primary_gpt[FWUP_BLOCK_SIZE * 2]);
    create_partitions(partitions, num_blocks, &secondary_gpt[0]);

    // Create the GPT headers
    header.num_partitions = (GPT_PARTITION_TABLE_BLOCKS * FWUP_BLOCK_SIZE / GPT_PARTITION_SIZE);
    header.partition_crc = crc32buf((const char *) secondary_gpt, header.num_partitions * GPT_PARTITION_SIZE);
    const char *unverified_guid = cfg_getstr(cfg, "guid");
    if (!unverified_guid || string_to_uuid_me(unverified_guid, header.disk_guid) < 0)
        return -1;

    header.first_usable_lba = 1 + GPT_SIZE_BLOCKS;
    header.last_usable_lba = num_blocks - GPT_SIZE_BLOCKS - 1;

    // Primary GPT
    header.current_lba = 1;
    header.backup_lba = num_blocks - 1;
    header.partition_lba = 2;
    create_gpt_header(&header, &mbr_and_primary_gpt[FWUP_BLOCK_SIZE]);

    // Secondary GPT
    header.current_lba = num_blocks - 1;
    header.backup_lba = 1;
    header.partition_lba = num_blocks - GPT_SIZE_BLOCKS;
    create_gpt_header(&header, &secondary_gpt[GPT_SIZE - FWUP_BLOCK_SIZE]);

    *secondary_gpt_offset = header.partition_lba * FWUP_BLOCK_SIZE;

    return 0;
}
