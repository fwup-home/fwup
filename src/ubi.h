/*
 * Copyright 2026 Herman verschooten
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

#ifndef UBI_H
#define UBI_H

#include <stdint.h>
#include <stddef.h>

// Start an atomic update of total_bytes on a UBI volume character
// device (e.g. /dev/ubi0_3). Returns the fd or -1 on error.
int ubi_volume_update_start(const char *path, int64_t total_bytes);

// Write the next count bytes of the update. UBI only supports
// sequential writes while updating.
int ubi_volume_update_write(const char *path, int fd, const void *buf, size_t count);

// Close the volume. UBI commits the update if all declared bytes were
// written and marks the volume corrupted otherwise.
int ubi_volume_update_finish(const char *path, int fd);

#endif
