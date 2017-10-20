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

#include "../config.h"
#include "cfgprint.h"
#include "fwfile.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <archive_entry.h>
#include <sodium.h>

int fwfile_add_meta_conf(cfg_t *cfg, struct archive *a, const unsigned char *signing_key)
{
    char *configtxt;
    size_t configtxt_len;

    configtxt_len = fwup_cfg_to_string(cfg, &configtxt);
    if (configtxt_len == 0)
        ERR_RETURN("Could not create meta.conf contents");

    int rc = fwfile_add_meta_conf_str(configtxt, configtxt_len, a, signing_key);

    free(configtxt);
    return rc;
}

int fwfile_add_meta_conf_str(const char *configtxt, int configtxt_len,
                             struct archive *a, const unsigned char *signing_key)
{
    struct archive_entry *entry;

    // If the user passed in a signing key, sign the meta.conf.
    if (signing_key) {
        unsigned char signature[crypto_sign_BYTES];
        crypto_sign_detached(signature, NULL,
                             (unsigned char *) configtxt, configtxt_len,
                             signing_key);

        entry = archive_entry_new();
        archive_entry_set_pathname(entry, "meta.conf.ed25519");
        archive_entry_set_size(entry, sizeof(signature));
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(a, entry);
        archive_write_data(a, signature, sizeof(signature));
        archive_entry_free(entry);
    }

    // Add meta.conf
    entry = archive_entry_new();
    archive_entry_set_pathname(entry, "meta.conf");
    archive_entry_set_size(entry, configtxt_len);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, configtxt, configtxt_len);
    archive_entry_free(entry);

    return 0;
}
