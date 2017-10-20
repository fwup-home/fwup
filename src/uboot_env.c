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

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

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

    return 0;
}

int uboot_env_create_cfg(cfg_t *cfg, struct uboot_env *output)
{
    output->block_offset = cfg_getint(cfg, "block-offset");
    output->block_count = cfg_getint(cfg, "block-count");
    output->env_size = output->block_count * FWUP_BLOCK_SIZE;
    output->vars = NULL;

    // This condition should only be hit if the .fw file was manually
    // modified, since the block-count should have been validated at
    // .fw creation time.
    if (output->block_count <= 0 || output->block_count >= UINT16_MAX)
        ERR_RETURN("invalid u-boot environment block count");

    return 0;
}

int uboot_env_read(struct uboot_env *env, const char *buffer)
{
    uboot_env_free(env);

    uint32_t expected_crc32 = ((uint8_t) buffer[0] | ((uint8_t) buffer[1] << 8) | ((uint8_t) buffer[2] << 16) | ((uint8_t) buffer[3] << 24));
    uint32_t actual_crc32 = crc32buf(buffer + 4, env->env_size - 4);
    if (expected_crc32 != actual_crc32)
        ERR_RETURN("U-boot environment (block %" PRIu64 ") CRC32 mismatch (expected 0x%08x; got 0x%08x)", env->block_offset, expected_crc32, actual_crc32);

    const char *end = buffer + env->env_size;
    const char *name = buffer + 4;
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

int uboot_env_write(struct uboot_env *env, char *buffer)
{
    if (env->env_size < 8)
        ERR_RETURN("u-boot environment block size too small");

    // U-boot environment blocks are filled by 0xff by default
    memset(buffer, 0xff, env->env_size);

    // Skip over the CRC until the end.
    char *p = buffer + 4;
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
    uint32_t crc32 = crc32buf(buffer + 4, env->env_size - 4);
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
