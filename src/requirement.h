/*
 * Copyright 2016 Frank Hunleth
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

#ifndef REQUIREMENT_H
#define REQUIREMENT_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>
#include <confuse.h>

#define REQ_MAX_ARGS  (10)

struct fun_context;
struct fat_cache;

struct req_context {
    // Function name and arguments
    int argc;
    const char *argv[REQ_MAX_ARGS];

    // Root meta.conf configuration
    cfg_t *cfg;

    // Task configuration
    cfg_t *task;

    // Function context
    struct fun_context *fctx;

    // Callback for getting a fat_cache handle for use with the fatfs code.
    int (*fatfs_ptr)(struct fun_context *fctx, off_t block_offset, struct fat_cache **fc);

    // Output file descriptor. <= 0 if not opened. (stdin is never ok)
    int output_fd;
};

int req_validate(struct req_context *rctx);
int req_requirement_met(struct req_context *rctx);
int req_apply_reqlist(struct req_context *rctx, cfg_opt_t *reqlist, int (*req)(struct req_context *rctx));

#endif // REQUIREMENT_H
