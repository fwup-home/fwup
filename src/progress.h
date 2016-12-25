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

#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdint.h>

/**
 * @brief How to report progress to the user
 */
enum fwup_progress_option {
    PROGRESS_MODE_OFF,
    PROGRESS_MODE_NUMERIC,
    PROGRESS_MODE_NORMAL,
    PROGRESS_MODE_FRAMING
};

struct fwup_progress {
    // If we're showing progress when applying, this is the number of progress_units that
    // should be 100%.
    int64_t total_units;

    // This counts up as we make progress.
    int64_t current_units;

    // The most recent progress reported is cached to avoid unnecessary context switching/IO
    int last_reported;

    // This is the starting progress value (normally 0 for 0%)
    int low;

    // This is the number of progress values (normally 100 to get from 0% to 100%)
    int range;

    // If the mode supports it, this is the start time of the update in milliseconds
    int start_time;
};

void progress_init(struct fwup_progress *progress, int progress_low, int progress_high);
void progress_report(struct fwup_progress *progress, int progress_units);
void progress_report_complete(struct fwup_progress *progress);

#endif // PROGRESS_H
