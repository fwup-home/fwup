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

#include "fwup_apply.h"
#include "util.h"
#include "cfgfile.h"

#include <archive.h>
#include <archive_entry.h>
#include <confuse.h>
#include <string.h>

int fwup_apply(const char *fw_filename, const char *output_filename)
{
    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);
    int rc = archive_read_open_filename(a, fw_filename, 16384);
    if (rc != ARCHIVE_OK)
        ERR_RETURN("Cannot open archive");

    struct archive_entry *ae;
    rc = archive_read_next_header(a, &ae);
    if (rc != ARCHIVE_OK)
        ERR_RETURN("Error reading archive");

    if (strcmp(archive_entry_pathname(ae), "meta.conf") != 0)
        ERR_RETURN("Expecting meta.conf to be first file");

    cfg_t *cfg;
    if (cfgfile_parse_fw_ae(a, ae, &cfg) < 0)
        return -1;

    archive_read_free(a);
    return 0;
}
