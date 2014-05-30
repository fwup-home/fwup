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

#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stddef.h>
#include <sys/types.h>
#include <confuse.h>

enum fun_context_type {
    FUN_CONTEXT_INIT,
    FUN_CONTEXT_FINISH,
    FUN_CONTEXT_ERROR,
    FUN_CONTEXT_FILE
};

#define FUN_MAX_ARGS  (10)
struct fun_private;

struct fun_context {
    // Context of where the function is called
    enum fun_context_type type;

    // Function name and arguments
    int argc;
    const char *argv[FUN_MAX_ARGS];

    // If the context supplies data, this is the expected byte count
    size_t expected_bytecount;

    // If the context supplies data, this function gets it. If read returns 0,
    // no more data is available. If <0, then there's an error.
    int (*read)(struct fun_context *fctx, const void **buffer, size_t *len, int64_t *offset);

    // Callback for reporting progress
    void (*report_progress)(struct fun_context *fctx, int progress_units);

    // Callback for getting a file handle for use with the fatfs code.
    int (*fatfs_ptr)(struct fun_context *fctx, int64_t block_offset, FILE **fatfs);

    // Output file descriptor. <= 0 if not opened. (stdin is never ok)
    int output_fd;

    void *cookie;
};

int fun_validate(struct fun_context *fctx);
int fun_run(struct fun_context *fctx);
int fun_run_funlist(struct fun_context *fctx, cfg_opt_t *funlist);
int fun_calc_progress_units(struct fun_context *fctx, int *progress_units);

#endif // FUNCTIONS_H
