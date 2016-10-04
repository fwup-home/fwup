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

#include "mbr.h"
#include "util.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Hardcode the cylinder/head/sector geometry, since it's not relevant for
// the types of memory that we use.
#define SECTORS_PER_HEAD    63
#define HEADS_PER_CYLINDER  255

/**
 * @brief mbr_verify check that the specified partitions make sense and don't overlap
 * @param partitions the partitions
 * @return 0 if successful
 */
int mbr_verify(const struct mbr_partition partitions[4])
{
    int i;
    // Check for overlap
    for (i = 0; i < 4; i++) {
        if (partitions[i].partition_type > 0xff || partitions[i].partition_type < 0)
            ERR_RETURN("invalid partition type");

        uint32_t ileft = partitions[i].block_offset;
        uint32_t iright = ileft + partitions[i].block_count;

        // Check if empty.
        if (partitions[i].partition_type == 0 ||
                ileft == iright)
            continue;

        int j;
        for (j = 0; j < 4; j++) {
            if (j == i)
                continue;

            uint32_t jleft = partitions[j].block_offset;
            uint32_t jright = jleft + partitions[j].block_count;

            if (partitions[j].partition_type == 0 ||
                    jleft == jright)
                continue;

            if ((ileft >= jleft && ileft < jright) ||
                (iright > jleft && iright <= jright))
                ERR_RETURN("partitions %d and %d overlap", i, j);
        }
    }

    return 0;
}

static void lba_to_chs(uint32_t lba, uint8_t *output)
{
    // If the block offset can't be represented in CHS form,
    // don't bother since it's mostly likely not used anyway.
    if (lba <= (SECTORS_PER_HEAD * HEADS_PER_CYLINDER * 0x3ff)) {
        uint16_t cylinder = lba / (SECTORS_PER_HEAD * HEADS_PER_CYLINDER);
        uint8_t head = (uint8_t) ((lba / SECTORS_PER_HEAD) % HEADS_PER_CYLINDER);
        uint8_t sector = (uint8_t) (lba % SECTORS_PER_HEAD) + 1;

        output[0] = head;
        output[1] = ((cylinder & 0x300) >> 2) | sector;
        output[2] = (cylinder & 0xff);
    }
}

static void copy_le32(uint8_t *output, uint32_t v)
{
    output[0]  = v & 0xff;
    output[1]  = (v >> 8) & 0xff;
    output[2] = (v >> 16) & 0xff;
    output[3] = (v >> 24) & 0xff;
}

static void copy_le16(uint8_t *output, uint16_t v)
{
    output[0]  = v & 0xff;
    output[1]  = (v >> 8) & 0xff;
}

static int create_partition(const struct mbr_partition *partition, uint8_t *output)
{
    // Clear out the partition entry
    memset(output, 0, 16);

    // Don't write most of the partition entry if it is unused.
    if (partition->partition_type > 0) {
        output[0] = partition->boot_flag ? 0x80 : 0x00;

        lba_to_chs(partition->block_offset, &output[1]);

        output[4] = partition->partition_type;
        lba_to_chs(partition->block_offset + partition->block_count - 1, &output[5]);
    }

    // There's an ugly hack use case where data is stored in the block offset and
    // count of unused partition entries. That's why the following two lines aren't
    // in the "if" block above.
    copy_le32(&output[8], partition->block_offset);
    copy_le32(&output[12], partition->block_count);

    return 0;
}

static int write_osip(const struct osip_header *osip, uint8_t *output)
{
    // OSIP Signature "$OS$"
    output[0] = '$';
    output[1] = 'O';
    output[2] = 'S';
    output[3] = '$';

    output[4] = 0; // Reserved

    output[5] = osip->minor; // Header minor revision
    output[6] = osip->major; // Header major revision
    output[7] = 0; // Placeholder for header checksum
    output[8] = osip->num_pointers;
    output[9] = osip->num_images;

    uint16_t header_size = 32 + 24 * osip->num_images;
    copy_le16(&output[10], header_size);

    memset(&output[12], 0, 20); // Reserved

    int i;
    uint8_t *osii_out = &output[32];
    for (i = 0; i < osip->num_images; i++) {
        const struct osii *descriptor = &osip->descriptors[i];
        copy_le16(&osii_out[0], descriptor->os_minor);
        copy_le16(&osii_out[2], descriptor->os_major);
        copy_le32(&osii_out[4], descriptor->start_block_offset);
        copy_le32(&osii_out[8], descriptor->ddr_load_address);
        copy_le32(&osii_out[12], descriptor->entry_point);
        copy_le32(&osii_out[16], descriptor->image_size);
        osii_out[20] = descriptor->attribute;
        memset(&osii_out[21], 0, 3); // Reserved

        osii_out += 24;
    }

    // Calculate and save the checksum
    uint8_t sum = output[0];
    for (i = 1; i < header_size; i++)
        sum = sum ^ output[i];
    output[7] = sum;

    return 0;
}

/**
 * @brief Create a master boot record and put it in output
 * @param partitions the list of partitions
 * @param bootstrap optional bootstrap code (must be 440 bytes or NULL if none)
 * @param osip optional OSIP header (NULL if none)
 * @param output the output location
 * @return 0 if success
 */
int mbr_create(const struct mbr_partition partitions[4],
               const uint8_t *bootstrap,
               const struct osip_header *osip,
               uint8_t output[512])
{
    if (bootstrap && osip->include_osip)
        ERR_RETURN("Can't specify both bootstrap and OSIP in MBR");

    if (mbr_verify(partitions) < 0)
        return -1;

    memset(output, 0, 512);
    if (bootstrap)
        memcpy(output, bootstrap, 440);

    if (osip->include_osip && write_osip(osip, output) < 0)
        return -1;

    int i;
    for (i = 0; i < 4; i++) {
        if (create_partition(&partitions[i], &output[446 + i * 16]) < 0)
            return -1;
    }

    output[510] = 0x55;
    output[511] = 0xaa;
    return 0;
}

static int read_partition(const uint8_t *input, struct mbr_partition *partition)
{
    if (input[0] & 0x80)
        partition->boot_flag = true;
    else
        partition->boot_flag = false;

    partition->partition_type = input[4];

    partition->block_offset =
            (input[8]) | (input[9] << 8) | (input[10] << 16) | (input[11] << 24);
    partition->block_count =
            (input[12]) | (input[13] << 8) | (input[14] << 16) | (input[15] << 24);

    return 0;
}

/**
 * @brief Decode the MBR data found in input
 * @param input the 512 byte MBR
 * @param partitions decoded data from the 4 partitions in the MBR
 * @return 0 if successful
 */
int mbr_decode(const uint8_t input[512], struct mbr_partition partitions[4])
{
    memset(partitions, 0, 4 * sizeof(struct mbr_partition));

    if (input[510] != 0x55 || input[511] != 0xaa)
        ERR_RETURN("MBR signature missing");

    int i;
    for (i = 0; i < 4; i++) {
        if (read_partition(&input[446 + i * 16], &partitions[i]) < 0)
            return -1;
    }

    return 0;
}

static int mbr_cfg_to_partitions(cfg_t *cfg, struct mbr_partition *partitions, int *found_partitions)
{
    cfg_t *partition;
    int i = 0;
    int found = 0;

    memset(partitions, 0, 4 * sizeof(struct mbr_partition));

    while ((partition = cfg_getnsec(cfg, "partition", i++)) != NULL) {
        int partition_ix = strtoul(cfg_title(partition), NULL, 0);
        if (partition_ix < 0 || partition_ix >= 4)
            ERR_RETURN("partition must be numbered 0 through 3");

        if (found & (1 << partition_ix))
            ERR_RETURN("invalid or duplicate partition number found");
        found = found | (1 << partition_ix);

        int unverified_type = cfg_getint(partition, "type");
        if (unverified_type < 0 || unverified_type > 0xff)
            ERR_RETURN("partition type must be between 0 and 255");

        partitions[partition_ix].partition_type = unverified_type;

        const char *unverified_block_offset = cfg_getstr(partition, "block-offset");
        if (!unverified_block_offset)
            ERR_RETURN("partition's block_offset is required");
        unsigned long block_offset = strtoul(unverified_block_offset, 0, 0);

        // strtoul returns error by returning ULONG_MAX and setting errno.
        // Values bigger than 2^32-1 won't fit in the MBR, so report an
        // error for those too.
        if ((block_offset == ULONG_MAX && errno != 0) || block_offset >= UINT32_MAX)
            ERR_RETURN("block_offset must be positive and less than 2^32 - 1: '%s'", unverified_block_offset);
        partitions[partition_ix].block_offset = block_offset;

        partitions[partition_ix].block_count = cfg_getint(partition, "block-count");
        if (partitions[partition_ix].block_count >= INT32_MAX)
            ERR_RETURN("block-count must be specified and less than 2^31 - 1");

        partitions[partition_ix].boot_flag = cfg_getbool(partition, "boot");
    }

    if (found_partitions)
        *found_partitions = found;
    return 0;
}

static int mbr_cfg_to_osip(cfg_t *cfg, struct osip_header *osip)
{
    cfg_t *osii;
    int i = 0;
    int found = 0;
    int largest_osii_ix = -1;

    memset(osip, 0, sizeof(struct osip_header));

    osip->include_osip = cfg_getbool(cfg, "include-osip");
    if (!osip->include_osip)
        return 0;

    osip->major = cfg_getint(cfg, "osip-major");
    osip->minor = cfg_getint(cfg, "osip-minor");
    osip->num_pointers = cfg_getint(cfg, "osip-num-pointers");

    while ((osii = cfg_getnsec(cfg, "osii", i++)) != NULL) {
        int osii_ix = strtoul(cfg_title(osii), NULL, 0);
        if (osii_ix < 0 || osii_ix >= 15)
            ERR_RETURN("osii must be numbered 0 through 15");

        if (found & (1 << osii_ix))
            ERR_RETURN("invalid or duplicate osii number found");
        found = found | (1 << osii_ix);

        if (osii_ix > largest_osii_ix)
            largest_osii_ix = osii_ix;

        osip->descriptors[osii_ix].os_major = cfg_getint(osii, "os-major");
        osip->descriptors[osii_ix].os_minor = cfg_getint(osii, "os-minor");

        osip->descriptors[osii_ix].start_block_offset = cfg_getint(osii, "start-block-offset");
        osip->descriptors[osii_ix].ddr_load_address = cfg_getint(osii, "ddr-load-address");
        osip->descriptors[osii_ix].entry_point = cfg_getint(osii, "entry-point");
        osip->descriptors[osii_ix].image_size = cfg_getint(osii, "image-size-blocks");
        osip->descriptors[osii_ix].attribute = cfg_getint(osii, "attribute");

    }
    osip->num_images = largest_osii_ix + 1;
    if (osip->num_images == 0)
        ERR_RETURN("need to specify one or more osii");

    return 0;
}
int mbr_verify_cfg(cfg_t *cfg)
{
    int found_partitions = 0;
    struct mbr_partition partitions[4];
    struct osip_header osip;

    const char *bootstrap_hex = cfg_getstr(cfg, "bootstrap-code");
    if (bootstrap_hex) {
        if (strlen(bootstrap_hex) != 440 * 2)
            ERR_RETURN("bootstrap-code should be exactly 440 bytes");
    }

    if (mbr_cfg_to_osip(cfg, &osip) < 0)
        return -1;

    if (osip.include_osip && bootstrap_hex)
        ERR_RETURN("cannot specify OSIP if including bootstrap code");

    if (mbr_cfg_to_partitions(cfg, partitions, &found_partitions) < 0)
        return -1;
    if (found_partitions == 0)
        ERR_RETURN("empty partition table?");

    return mbr_verify(partitions);
}


int mbr_create_cfg(cfg_t *cfg, uint8_t output[512])
{
    struct mbr_partition partitions[4];
    struct osip_header osip;

    if (mbr_cfg_to_partitions(cfg, partitions, NULL) < 0)
        return -1;

    if (mbr_cfg_to_osip(cfg, &osip) < 0)
        return -1;

    uint8_t bootstrap[440];
    memset(bootstrap, 0, sizeof(bootstrap));

    const char *bootstrap_hex = cfg_getstr(cfg, "bootstrap-code");
    if (bootstrap_hex &&
        (hex_to_bytes(bootstrap_hex, bootstrap, sizeof(bootstrap)) < 0))
        return -1;

    return mbr_create(partitions, bootstrap_hex ? bootstrap : NULL, &osip, output);
}
