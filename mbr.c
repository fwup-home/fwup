#include "mbr.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Hardcode the cylinder/head/sector geometry, since it's not relevant for
// the types of memory that we use.
#define SECTORS_PER_HEAD    63
#define HEADS_PER_CYLINDER  255

static const char *last_error = NULL;

static int check_partitions(const struct mbr_partition partitions[4])
{
    int i;
    // Check for overlap
    for (i = 0; i < 4; i++) {
        if (partitions[i].partition_type > 0xff || partitions[i].partition_type < 0) {
            last_error = "invalid partition type";
            return -1;
        }

        int ileft = partitions[i].block_offset;
        int iright = ileft + partitions[i].block_count - 1;
        int j;
        for (j = 0; j < 4; j++) {
            if (j == i)
                continue;

            int jleft = partitions[j].block_offset;
            int jright = jleft + partitions[j].block_count - 1;

            if ((ileft >= jleft && ileft <= jright) ||
                (iright >= jleft && iright <= jright)) {
                last_error = "partitions overlap";
                return -1;
            }
        }
    }

    return 0;
}

static int lba_to_chs(uint32_t lba, char *output)
{
    uint16_t cylinder = lba / (SECTORS_PER_HEAD * HEADS_PER_CYLINDER);
    uint8_t head = (uint8_t) ((lba / SECTORS_PER_HEAD) % HEADS_PER_CYLINDER);
    uint8_t sector = (uint8_t) (lba % SECTORS_PER_HEAD) + 1;

    output[0] = head;
    output[1] = ((cylinder & 0x300) >> 2) | sector;
    output[2] = (cylinder & 0xff);

    return 0;
}

static int create_partition(const struct mbr_partition *partition, char *output)
{
    output[0] = partition->boot_flag ? 0x80 : 0x00;

    if (lba_to_chs(partition->block_offset, &output[1]) < 0)
        return -1;

    output[4] = partition->partition_type;

    if (lba_to_chs(partition->block_offset + partition->block_count - 1, &output[5]) < 0)
        return -1;

    output[8]  = partition->block_offset & 0xff;
    output[9]  = (partition->block_offset >> 8) & 0xff;
    output[10] = (partition->block_offset >> 16) & 0xff;
    output[11] = (partition->block_offset >> 24) & 0xff;

    output[12] = partition->block_count & 0xff;
    output[13] = (partition->block_count >> 8) & 0xff;
    output[14] = (partition->block_count >> 16) & 0xff;
    output[15] = (partition->block_count >> 24) & 0xff;

    return 0;
}

int mbr_create(struct mbr_partition partitions[4], const char *bootstrap, char output[512])
{
    if (check_partitions(partitions) < 0)
        return -1;

    memset(output, 0, 512);
    if (bootstrap)
        memcpy(output, bootstrap, 440);

    int i;
    for (i = 0; i < 4; i++) {
        if (create_partition(&partitions[i], &output[446 + i * 16]) < 0)
            return -1;
    }

    output[510] = 0x55;
    output[511] = 0xaa;
    return 0;
}

const char *mbr_last_error()
{
    return last_error;
}
