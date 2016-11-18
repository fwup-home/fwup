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

/**
 * @brief How to report progress to the user
 */
enum fwup_progress_mode {
    PROGRESS_MODE_OFF,
    PROGRESS_MODE_NUMERIC,
    PROGRESS_MODE_NORMAL,
    PROGRESS_MODE_FRAMING
};

struct fwup_progress {
    // This is set based on command line parameters
    enum fwup_progress_mode mode;

    // If we're showing progress when applying, this is the number of progress_units that
    // should be 100%.
    int total_units;

    // This counts up as we make progress.
    int current_units;

    // The most recent progress reported is cached to avoid unnecessary context switching/IO
    int last_reported;
};

void progress_init(struct fwup_progress *progress, enum fwup_progress_mode mode);
void progress_report(struct fwup_progress *progress, int progress_units);
void progress_report_complete(struct fwup_progress *progress);

#endif // PROGRESS_H
