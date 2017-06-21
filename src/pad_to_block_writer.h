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
