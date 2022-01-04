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

#include "uboot_env.h"
#include "util.h"
#include "crc32.h"
#include "block_cache.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Licensing note: U-boot is licensed under the GPL which is incompatible with
//                 fwup's Apache 2.0 license. Please don't copy code from
//                 U-boot especially since the U-boot environment data
//                 structure is simple enough to reverse engineer by playing
//                 with mkenvimage.

int uboot_env_verify_cfg(cfg_t *cfg)
{
    int block_offset = cfg_getint(cfg, "block-offset");
    if (block_offset < 0)
        ERR_RETURN("block-offset must be specified and less than 2^31 - 1");

    int block_count = cfg_getint(cfg, "block-count");
    if (block_count <= 0 || block_count >= UINT16_MAX)
        ERR_RETURN("block-count must be specified, greater than 0 and less than 2^16 - 1");

    int block_offset_redund = cfg_getint(cfg, "block-offset-redund");
    if (block_offset_redund >= 0) {
        if (block_offset_redund >= INT32_MAX)
            ERR_RETURN("block-offset-redund must be less than 2^31 - 1");

        int redundant_left = block_offset_redund;
        int redundant_right = block_offset_redund + block_count;
        if ((redundant_left >= block_offset && redundant_left < block_offset + block_count) ||
            (redundant_right > block_offset && redundant_right < block_offset + block_count))
            ERR_RETURN("block-offset-redund can't overlap primary U-Boot environment");
    }

    return 0;
}

int uboot_env_block_count(cfg_t *cfg)
{
    // This assumes a valid configuration as checked by uboot_env_verify_cfg.
    int block_count = cfg_getint(cfg, "block-count");

    int block_offset_redund = cfg_getint(cfg, "block-offset-redund");
    if (block_offset_redund >= 0)
        block_count = block_count * 2;

    return block_count;
}

int uboot_env_create_cfg(cfg_t *cfg, struct uboot_env *output)
{
    memset(output, 0, sizeof(struct uboot_env));

    output->block_offset = cfg_getint(cfg, "block-offset");
    output->block_count = cfg_getint(cfg, "block-count");
    output->env_size = output->block_count * FWUP_BLOCK_SIZE;

    int redundant_offset = cfg_getint(cfg, "block-offset-redund");
    if (redundant_offset >= 0) {
        output->use_redundant = true;
        output->redundant_block_offset = redundant_offset;
        output->write_primary = true;
        output->write_secondary = true;
        output->flags = 0;
    } else {
        output->use_redundant = false;
        output->redundant_block_offset = output->block_offset;
        output->write_primary = true;
    }

    output->vars = NULL;

    // This condition should only be hit if the .fw file was manually
    // modified, since the block-count should have been validated at
    // .fw creation time.
    if (output->block_count == 0 || output->block_count >= UINT16_MAX)
        ERR_RETURN("invalid u-boot environment block count");

    return 0;
}

static int uboot_env_decode(struct uboot_env *env, const char *buffer)
{
    uboot_env_free(env);

    size_t data_offset = (env->use_redundant ? 5 : 4);
    uint32_t expected_crc32 = ((uint8_t) buffer[0] | ((uint8_t) buffer[1] << 8) | ((uint8_t) buffer[2] << 16) | ((uint8_t) buffer[3] << 24));
    uint32_t actual_crc32 = crc32buf(buffer + data_offset, env->env_size - data_offset);
    if (expected_crc32 != actual_crc32)
        ERR_RETURN("U-boot environment (block %" PRIu64 ") CRC32 mismatch (expected 0x%08x; got 0x%08x)", env->block_offset, expected_crc32, actual_crc32);

    const char *end = buffer + env->env_size;
    const char *name = buffer + data_offset;
    while (name != end && *name != '\0') {
        const char *endname = name + 1;
        for (;;) {
            if (endname == end || *endname == '\0')
                ERR_RETURN("Invalid U-boot environment");

            if (*endname == '=')
                break;

            endname++;
        }

        const char *value = endname + 1;
        const char *endvalue = value;
        for (;;) {
            if (endvalue == end)
                ERR_RETURN("Invalid U-boot environment");

            if (*endvalue == '\0')
                break;

            endvalue++;
        }

        struct uboot_name_value *pair = malloc(sizeof(struct uboot_name_value));
        pair->name = strndup(name, endname - name);
        pair->value = strndup(value, value - endvalue);
        pair->next = env->vars;
        env->vars = pair;

        name = endvalue + 1;
    }

    return 0;
}

int uboot_env_setenv(struct uboot_env *env, const char *name, const char *value)
{
    struct uboot_name_value *pair;
    for (pair = env->vars; pair != NULL; pair = pair->next) {
        if (strcmp(pair->name, name) == 0) {
            free(pair->value);
            pair->value = strdup(value);
            return 0;
        }
    }
    pair = malloc(sizeof(*pair));
    pair->name = strdup(name);
    pair->value = strdup(value);
    pair->next = env->vars;
    env->vars = pair;
    return 0;
}

int uboot_env_unsetenv(struct uboot_env *env, const char *name)
{
    struct uboot_name_value *prev = NULL;
    struct uboot_name_value *pair;
    for (pair = env->vars;
         pair != NULL;
         prev = pair, pair = pair->next) {
        if (strcmp(pair->name, name) == 0) {
            if (prev)
                prev->next = pair->next;
            else
                env->vars = pair->next;
            free(pair->name);
            free(pair->value);
            free(pair);
            break;
        }
    }
    return 0;
}

int uboot_env_getenv(struct uboot_env *env, const char *name, char **value)
{
    struct uboot_name_value *pair;
    for (pair = env->vars; pair != NULL; pair = pair->next) {
        if (strcmp(pair->name, name) == 0) {
            *value = strdup(pair->value);
            return 0;
        }
    }

    *value = NULL;
    ERR_RETURN("variable '%s' not found", name);
}

static int env_name_compare(const void *a, const void *b)
{
    struct uboot_name_value **apair = (struct uboot_name_value **) a;
    struct uboot_name_value **bpair = (struct uboot_name_value **) b;

    return strcmp((*apair)->name, (*bpair)->name);
}

static void uboot_env_sort(struct uboot_env *env)
{
    int count = 0;
    struct uboot_name_value *pair;
    for (pair = env->vars; pair != NULL; pair = pair->next)
        count++;

    if (count == 0)
        return;

    struct uboot_name_value **pairarray =
        (struct uboot_name_value **) malloc(count * sizeof(struct uboot_name_value *));
    int i;
    for (pair = env->vars, i = 0; pair != NULL; pair = pair->next, i++)
        pairarray[i] = pair;

    qsort(pairarray, count, sizeof(struct uboot_name_value *), env_name_compare);

    for (i = 0; i < count - 1; i++)
        pairarray[i]->next = pairarray[i + 1];
    pairarray[count - 1]->next = NULL;
    env->vars = pairarray[0];

    free(pairarray);
}

static int uboot_env_encode(struct uboot_env *env, char *buffer)
{
    // U-boot environment blocks are filled by 0xff by default
    memset(buffer, 0xff, env->env_size);

    // Skip over the CRC until the end.
    size_t data_offset = (env->use_redundant ? 5 : 4);
    char *p = buffer + data_offset;
    char *end = buffer + env->env_size - 2;

    // Sort the name/value pairs so that their ordering is
    // deterministic.
    uboot_env_sort(env);

    // Add all of the name/value pairs.
    struct uboot_name_value *pair;
    for (pair = env->vars; pair != NULL; pair = pair->next) {
        size_t namelen = strlen(pair->name);
        size_t valuelen = strlen(pair->value);
        if (p + namelen + 1 + valuelen >= end)
            ERR_RETURN("Not enough room in U-boot environment");

        memcpy(p, pair->name, namelen);
        p += namelen;
        *p = '=';
        p++;
        memcpy(p, pair->value, valuelen);
        p += valuelen;
        *p = 0;
        p++;
    }

    // Add the extra NULL byte on the end.
    *p = 0;

    // Calculate and add the CRC-32
    uint32_t crc32 = crc32buf(buffer + data_offset, env->env_size - data_offset);
    buffer[0] = crc32 & 0xff;
    buffer[1] = (crc32 >> 8) & 0xff;
    buffer[2] = (crc32 >> 16) & 0xff;
    buffer[3] = crc32 >> 24;

    return 0;
}

void uboot_env_free(struct uboot_env *env)
{
    struct uboot_name_value *pair = env->vars;
    while (pair != NULL) {
        free(pair->name);
        free(pair->value);

        struct uboot_name_value *next = pair->next;
        free(pair);
        pair = next;
    }

    env->vars = NULL;
}

static int uboot_env_read_non_redundant(struct block_cache *bc, struct uboot_env *env)
{
    int rc;
    char *buffer = (char *) malloc(env->env_size);

    // Initialize data that's only used for redundant environment support
    env->write_primary = true;
    env->write_secondary = false;
    env->flags = 0;

    OK_OR_CLEANUP_MSG(block_cache_pread(bc, buffer, env->env_size, env->block_offset * FWUP_BLOCK_SIZE),
                      "unexpected error reading uboot environment: %s", strerror(errno));

    rc = uboot_env_decode(env, buffer);
cleanup:
    free(buffer);
    return rc;
}

static int uboot_env_read_redundant(struct block_cache *bc, struct uboot_env *env)
{
    int rc;
    char *buffer1 = (char *) malloc(env->env_size);
    char *buffer2 = (char *) malloc(env->env_size);

    // Reset which environment to write until we see which one was read.
    env->write_primary = false;
    env->write_secondary = false;
    env->flags = 0;

    OK_OR_CLEANUP_MSG(block_cache_pread(bc, buffer1, env->env_size, env->block_offset * FWUP_BLOCK_SIZE),
                      "unexpected error reading primary uboot environment: %s", strerror(errno));

    OK_OR_CLEANUP_MSG(block_cache_pread(bc, buffer2, env->env_size, env->redundant_block_offset * FWUP_BLOCK_SIZE),
                      "unexpected error reading redundant uboot environment: %s", strerror(errno));

    // The flags really are a counter. The bigger one (accounting for wrap) determines
    // which was written last and should be tried first.
    int8_t flag1 = buffer1[4];
    int8_t flag2 = buffer2[4];

    if (flag1 - flag2 >= 0) {
        // Check the first environment first
        rc = uboot_env_decode(env, buffer1);
        if (rc == 0) {
            env->flags = flag1;
            env->write_secondary = true;
        } else {
            env->flags = flag2;
            env->write_primary = true;
            rc = uboot_env_decode(env, buffer2);
            if (rc < 0)
                env->write_secondary = true;
        }
    } else {
        // Check the redundant environment first
        rc = uboot_env_decode(env, buffer2);
        if (rc == 0) {
            env->flags = flag2;
            env->write_primary = true;
        } else {
            env->flags = flag1;
            env->write_secondary = true;
            rc = uboot_env_decode(env, buffer1);
            if (rc < 0)
                env->write_primary = true;
        }
    }

cleanup:
    free(buffer2);
    free(buffer1);
    return rc;
}

int uboot_env_read(struct block_cache *bc, struct uboot_env *env)
{
    if (env->use_redundant)
        return uboot_env_read_redundant(bc, env);
    else
        return uboot_env_read_non_redundant(bc, env);
}

int uboot_env_write(struct block_cache *bc, struct uboot_env *env)
{
    int rc = 0;
    char *buffer = (char *) malloc(env->env_size);

    OK_OR_CLEANUP(uboot_env_encode(env, buffer));

    if (env->use_redundant)
        buffer[4] = env->flags + 1;

    if (env->write_primary) {
        OK_OR_CLEANUP_MSG(block_cache_pwrite(bc, buffer, env->env_size, env->block_offset * FWUP_BLOCK_SIZE, false),
                          "unexpected error writing uboot environment: %s", strerror(errno));
    }

    if (env->write_secondary) {
        OK_OR_CLEANUP_MSG(block_cache_pwrite(bc, buffer, env->env_size, env->redundant_block_offset * FWUP_BLOCK_SIZE, false),
                          "unexpected error writing redundant uboot environment: %s", strerror(errno));
    }
cleanup:
    free(buffer);
    return rc;
}
