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

#ifndef FWUP_APPLY_H
#define FWUP_APPLY_H

#include "functions.h"

struct fwup_progress;

int fwup_apply(const char *fw_filename,
               const char *task,
               int output_fd,
               off_t end_offset,
               struct fwup_progress *progress,
               unsigned char *const* public_keys,
               bool enable_trim,
               bool verify_writes,
               bool minimize_writes);

#endif // FWUP_APPLY_H
