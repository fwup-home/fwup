/*
 * Copyright 2016-2017 Frank Hunleth
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

#ifndef ARCHIVE_OPEN_H
#define ARCHIVE_OPEN_H

#include <stdlib.h>
#include <stdint.h>

struct fwup_progress;
struct archive;

int fwup_archive_open_filename(struct archive *a, const char *filename, struct fwup_progress *progress);
int fwup_archive_read_data_block(struct archive *a, const void **buff, size_t *s, int64_t *o);

#endif // ARCHIVE_OPEN_H
