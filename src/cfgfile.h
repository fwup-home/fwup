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

#ifndef CFGFILE_H
#define CFGFILE_H

#include <confuse.h>

struct archive;
struct archive_entry;

int archive_read_all_data(struct archive *a, char *buffer, off_t total_size);

int cfgfile_parse_file(const char *filename, cfg_t **cfg);
int cfgfile_parse_fw_ae(struct archive *a, struct archive_entry *ae, cfg_t **cfg, unsigned char *meta_conf_signature, const unsigned char *public_key);
int cfgfile_parse_fw_meta_conf(const char *filename, cfg_t **cfg, const unsigned char *public_key);
void cfgfile_free(cfg_t *cfg);

#endif // CFGFILE_H
