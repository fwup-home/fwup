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

#include "fwup_metadata.h"
#include <confuse.h>
#include <stdlib.h>

#include "cfgfile.h"
#include "cfgprint.h"
#include "util.h"

static void list_one(cfg_t *cfg, const char *key, struct simple_string *s)
{
    fwup_cfg_opt_to_string(cfg_getopt(cfg, key), s);
}

static void list_metadata(cfg_t *cfg, struct simple_string *s)
{
    list_one(cfg, "meta-product", s);
    list_one(cfg, "meta-description", s);
    list_one(cfg, "meta-version", s);
    list_one(cfg, "meta-author", s);
    list_one(cfg, "meta-platform", s);
    list_one(cfg, "meta-architecture", s);
    list_one(cfg, "meta-creation-date", s);
    list_one(cfg, "meta-fwup-version", s);
}

/**
 * @brief Dump the metadata in a firmware update file
 * @param fw_filename the firmware update filename
 * @param public_key an option public key if checking signatures
 * @return 0 if successful
 */
int fwup_metadata(const char *fw_filename, const unsigned char *public_key)
{
    cfg_t *cfg;
    if (cfgfile_parse_fw_meta_conf(fw_filename, &cfg, public_key) < 0)
        return -1;

    struct simple_string s;
    simple_string_init(&s);
    list_metadata(cfg, &s);
    cfgfile_free(cfg);

    fwup_output(FRAMING_TYPE_SUCCESS, 0, s.str);
    free(s.str);
    return 0;
}
