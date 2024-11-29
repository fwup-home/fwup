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

static int is_extended_type(uint8_t type)
{
    return type == 0x05 || type == 0x0f;
}

/**
 * @brief mbr_verify check that the specified partitions make sense and don't overlap
 * @param table the partitions
 * @return 0 if successful
 */
static int mbr_verify(const struct mbr_table *table)
{
    bool expanding = false;
    int i;

    // Check for overlap
    for (i = 0; i < MBR_MAX_PARTITIONS; i++) {
        const struct mbr_partition *partition = &table->partitions[i];
        if (partition->partition_type > 0xff || partition->partition_type < 0)
            ERR_RETURN("invalid partition type");

        uint32_t ileft = partition->block_offset;
        uint32_t iright = ileft + partition->block_count;

        // Check if empty.
        if (partition->partition_type == 0)
            continue;

        if (ileft == iright && !partition->expand_flag)
            continue;

        // Validate that if expand is used, it has to be the last partition unless it's an extended partition
        if (expanding)
            ERR_RETURN("a partition can't be specified after the one with \"expand = true\"");

        if (partition->expand_flag && i != 3 && !is_extended_type(partition->partition_type))
            expanding = true;

        int j;
        for (j = i + 1; j < MBR_MAX_PARTITIONS; j++) {
            const struct mbr_partition *j_partition = &table->partitions[j];

            // Skip unused partitions
            if (j_partition->partition_type == 0)
                continue;

            uint32_t jleft = j_partition->block_offset;
            uint32_t jright = jleft + j_partition->block_count;

            // Skip 0-length partitions
            if (jleft == jright)
                continue;

            // If partition 3 is an extended partition, then it's required to contain the following partitions.
            bool overlap_required = (i == 3 && is_extended_type(partition->partition_type));

            bool partition_overlaps = ! (ileft >= jright || iright <= jleft);
            bool ebr_i_overlaps = (partition->record_offset > 0 && partition->record_offset >= jleft && partition->record_offset < jright);
            bool ebr_j_overlaps = (j_partition->record_offset > 0 && j_partition->record_offset >= ileft && j_partition->record_offset < iright);

            if (partition_overlaps != overlap_required) {
                if (!overlap_required)
                    ERR_RETURN("partitions %d (blocks %u to %u) and %d (blocks %u to %u) overlap",
                            i, ileft, iright, j, jleft, jright);
                else
                    ERR_RETURN("partition 3, the extended partition, is expected to contain partition %d", j);
            }

            if (ebr_j_overlaps != overlap_required) {
                if (!overlap_required)
                    ERR_RETURN("partition %d (blocks %u to %u) overlaps the EBR at %u for partition %d",
                            i, ileft, iright, j_partition->record_offset, j);
                else
                    ERR_RETURN("partition 3, the extended partition, is expected to contain the EBR for partition %d", j);
            }

            // Subtle: ebr_i_overlaps is guaranteed false when i == 3.
            if (ebr_i_overlaps)
                ERR_RETURN("partition %d (blocks %u to %u) overlaps the EBR at %u for partition %d",
                        j, jleft, jright, partition->record_offset, i);
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

static void expand_partition(const struct mbr_partition *in_part,
                             struct mbr_partition *out_part,
                             uint32_t num_blocks)
{
    *out_part = *in_part;

    if (in_part->expand_flag &&
        num_blocks > (in_part->block_offset + in_part->block_count))
        out_part->block_count = num_blocks - in_part->block_offset;
    else
        out_part->block_count = in_part->block_count;

    out_part->expand_flag = false;
}

static void expand_partitions(const struct mbr_table *input, struct mbr_table *output, uint32_t num_blocks)
{
    for (int i = 0; i < MBR_MAX_PARTITIONS; i++) {
        uint32_t offset = input->partitions[i].block_offset + input->partitions[i].block_count;
        if (offset > num_blocks)
            num_blocks = offset;
    }

    for (int i = 0; i < MBR_MAX_PARTITIONS; i++)
        expand_partition(&input->partitions[i], &output->partitions[i], num_blocks);
    output->num_extended_partitions = input->num_extended_partitions;
 }

static void create_partition(const struct mbr_partition *partition, uint8_t *output)
{
    uint32_t block_count = partition->block_count;

    // Write the partition entry
    if (partition->partition_type > 0) {
        output[0] = partition->boot_flag ? 0x80 : 0x00;

        lba_to_chs(partition->block_offset, &output[1]);

        output[4] = partition->partition_type;
        lba_to_chs(partition->block_offset + block_count - 1, &output[5]);
    } else {
        // Clear out an unused entry
        memset(output, 0, 8);
    }

    // There's an ugly hack use case where data is stored in the block offset and
    // count of unused partition entries. That's why the following two lines aren't
    // in the "if" block above.
    copy_le32(&output[8], partition->block_offset);
    copy_le32(&output[12], block_count);
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
 * @param table the list of partitions
 * @param bootstrap optional bootstrap code (must be 440 bytes or NULL if none)
 * @param osip optional OSIP header (NULL if none)
 * @param signature
 * @param num_blocks the total number of blocks in the storage or 0 if unknown
 * @param output where to store the raw bytes
 * @param output_count the number of bytes written to the output buffer
 * @return 0 if success
 */
static int mbr_create(const struct mbr_table *table,
                      const uint8_t *bootstrap,
                      const struct osip_header *osip,
                      uint32_t signature,
                      uint32_t num_blocks,
                      struct mbr_raw_partition *output,
                      uint32_t *output_count)
{
    if (bootstrap && osip->include_osip)
        ERR_RETURN("Can't specify both bootstrap and OSIP in MBR");

    struct mbr_table expanded_table;
    expand_partitions(table, &expanded_table, num_blocks);
    if (mbr_verify(&expanded_table) < 0)
        return -1;

    uint8_t *raw_mbr = output[0].data;
    output[0].block_offset = 0;
    if (bootstrap)
        memcpy(raw_mbr, bootstrap, 440);
    else
        memset(raw_mbr, 0, 440);

    if (osip->include_osip && write_osip(osip, raw_mbr) < 0)
        return -1;

    copy_le32(&raw_mbr[440], signature);

    // Not copy protected?
    raw_mbr[444] = 0;
    raw_mbr[445] = 0;

    for (int i = 0; i < MBR_MAX_PRIMARY_PARTITIONS; i++)
        create_partition(&expanded_table.partitions[i], &raw_mbr[446 + i * 16]);

    raw_mbr[510] = 0x55;
    raw_mbr[511] = 0xaa;
    *output_count = 1;

    for (int i = 0; i < expanded_table.num_extended_partitions; i++) {
        const struct mbr_partition *partition = &expanded_table.partitions[MBR_MAX_PRIMARY_PARTITIONS + i];

        output[i + 1].block_offset = partition->record_offset;

        uint8_t *ebr = output[i + 1].data;
        memset(ebr, 0, 446);

        struct mbr_partition part;

        part.boot_flag = false;
        part.expand_flag = false;
        part.partition_type = partition->partition_type;
        part.block_offset = partition->block_offset - output[i+1].block_offset; // delta offset to partition
        part.block_count = partition->block_count;
        create_partition(&part, &ebr[446 + 0 * 16]);

        if (i < expanded_table.num_extended_partitions - 1) {
            part.partition_type = 0xf;
            part.boot_flag = false;
            part.expand_flag = false;
            part.block_offset = i + 1;
            part.block_count = 1;
            create_partition(&part, &ebr[446 + 1 * 16]);
        } else {
            // Null next EBR
            memset(&ebr[446 + 1 * 16], 0, 32);
        }
        // The third and fourth partition slots aren't used.
        memset(&ebr[446 + 2 * 16], 0, 32);
        ebr[510] = 0x55;
        ebr[511] = 0xaa;
        *output_count += 1;
    }
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
 * @brief Decode the MBR partitions found in input
 * @param input the 512 byte MBR
 * @param table decoded data from the 4 partitions in the MBR
 * @return 0 if successful
 */
int mbr_decode(const uint8_t input[512], struct mbr_table *table)
{
    memset(table, 0, sizeof(struct mbr_table));

    if (input[510] != 0x55 || input[511] != 0xaa)
        ERR_RETURN("MBR signature missing");

    int i;
    for (i = 0; i < MBR_MAX_PRIMARY_PARTITIONS; i++) {
        if (read_partition(&input[446 + i * 16], &table->partitions[i]) < 0)
            return -1;
    }

    return 0;
}

static int mbr_cfg_to_partitions(cfg_t *cfg, struct mbr_table *table)
{
    cfg_t *partition_cfg;
    int i = 0;
    unsigned int found = 0;

    memset(table, 0, sizeof(struct mbr_table));

    while ((partition_cfg = cfg_getnsec(cfg, "partition", i++)) != NULL) {
        int partition_ix = strtoul(cfg_title(partition_cfg), NULL, 0);
        if (partition_ix < 0 || partition_ix >= MBR_MAX_PARTITIONS)
            ERR_RETURN("partition must be numbered 0 through %d", MBR_MAX_PARTITIONS - 1);

        if (found & (1 << partition_ix))
            ERR_RETURN("invalid or duplicate partition number found for %d", partition_ix);
        found = found | (1 << partition_ix);

        struct mbr_partition *partition = &table->partitions[partition_ix];
        if (partition_ix >= MBR_MAX_PRIMARY_PARTITIONS)
            table->num_extended_partitions = partition_ix - MBR_MAX_PRIMARY_PARTITIONS + 1;

        int unverified_type = cfg_getint(partition_cfg, "type");
        if (unverified_type < 0 || unverified_type > 0xff)
            ERR_RETURN("partition %d's type must be between 0 and 255", partition_ix);

        partition->partition_type = unverified_type;

        const char *unverified_block_offset = cfg_getstr(partition_cfg, "block-offset");
        if (!unverified_block_offset || *unverified_block_offset == '\0')
            ERR_RETURN("partition %d's block_offset is required", partition_ix);
        char *endptr;
        unsigned long block_offset = strtoul(unverified_block_offset, &endptr, 0);

        // strtoul returns error by returning ULONG_MAX and setting errno.
        // Values bigger than 2^32-1 won't fit in the MBR, so report an
        // error for those too.
        if ((block_offset == ULONG_MAX && errno != 0) || block_offset >= UINT32_MAX)
            ERR_RETURN("partition %d's block_offset must be positive and less than 2^32 - 1: '%s'", partition_ix, unverified_block_offset);
        if (*endptr != '\0')
            ERR_RETURN("error parsing partition %d's block offset", partition_ix);

        partition->block_offset = block_offset;
        partition->block_count = cfg_getint(partition_cfg, "block-count");
        partition->record_offset = 0;

        if (is_extended_type(unverified_type)) {
            // Check that the extended partition is the 4th primary partition
            if (partition_ix != 3)
                ERR_RETURN("only partition 3 may be specified as the extended partition");
            partition->boot_flag = false;
            partition->expand_flag = true;
            if (partition->block_count >= INT32_MAX)
            partition->block_count = 1;
        } else {
            // Normal partition
            partition->boot_flag = cfg_getbool(partition_cfg, "boot");
            partition->expand_flag = cfg_getbool(partition_cfg, "expand");
            if (partition->block_count >= INT32_MAX)
                ERR_RETURN("partition %d's block-count must be specified and less than 2^31 - 1", partition_ix);
        }
    }

    if (table->num_extended_partitions > 0) {
        if (!is_extended_type(table->partitions[3].partition_type))
            ERR_RETURN("to use more than 4 partitions with MBR, partition 3 must have extended partition type 0xf or 0x5");

        // Assign record offsets for all logical partitions
        uint32_t record_offset = table->partitions[3].block_offset;
        for (int i = MBR_MAX_PRIMARY_PARTITIONS;
             i < MBR_MAX_PARTITIONS && table->partitions[i].partition_type;
             i++) {
                fprintf(stderr, "record_offset: %d\n", record_offset);
            table->partitions[i].record_offset = record_offset;
            record_offset++;
        }
    }

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
    struct mbr_table table;
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

    if (mbr_cfg_to_partitions(cfg, &table) < 0)
        return -1;

    struct mbr_table expanded_table;
    expand_partitions(&table, &expanded_table, 0);
    return mbr_verify(&expanded_table);
}

/**
 * @brief Encode an MBR
 *
 * @param cfg the mbr configuration
 * @param num_blocks the number of blocks on the destination or 0 if unknown
 * @param output where to store the encoded MBR and any extended partitions
 * @param output_count the number of records used in the output buffer
 * @return 0 if successful
 */
int mbr_create_cfg(cfg_t *cfg, uint32_t num_blocks, struct mbr_raw_partition *output, uint32_t *output_count)
{
    struct mbr_table table;
    struct osip_header osip;

    if (mbr_cfg_to_partitions(cfg, &table) < 0)
        return -1;

    if (mbr_cfg_to_osip(cfg, &osip) < 0)
        return -1;

    uint8_t bootstrap[440];
    memset(bootstrap, 0, sizeof(bootstrap));

    const char *bootstrap_hex = cfg_getstr(cfg, "bootstrap-code");
    if (bootstrap_hex &&
        (hex_to_bytes(bootstrap_hex, bootstrap, sizeof(bootstrap)) < 0))
        return -1;

    const char *raw_signature = cfg_getstr(cfg, "signature");
    uint32_t signature = !raw_signature ? 0 : strtoul(raw_signature, NULL, 0);

    mbr_create(&table, bootstrap_hex ? bootstrap : NULL, &osip, signature, num_blocks, output, output_count);
    return 0;
}
