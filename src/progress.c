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

#include "progress.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <sys/time.h>

extern enum fwup_progress_option fwup_progress_mode;

// Elapsed time measurement maxes out at 2^31 ms = 24 days
// Despite not being monotic, gettimeofday is more portable, and
// getting the duration wrong is not the end of the world.
static int current_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static void output_progress(struct fwup_progress *progress, int to_report)
{
    if (to_report == progress->last_reported)
        return;

    progress->last_reported = to_report;

    switch (fwup_progress_mode) {
    case PROGRESS_MODE_NUMERIC:
        printf("%d\n", to_report);
        fflush(stdout);
        break;

    case PROGRESS_MODE_NORMAL:
        printf("\r%3d%%", to_report);
        fflush(stdout);
        break;

    case PROGRESS_MODE_FRAMING:
        fwup_output(FRAMING_TYPE_PROGRESS, to_report, "");
        break;

    case PROGRESS_MODE_OFF:
    default:
        break;
    }
}

/**
 * @brief Initialize progress reporting
 *
 * @param progress the progress state struct
 * @param progress_low the lowest progress value (normally 0)
 * @param progress_high the highest progress value (normally 100)
 *
 * This function also immediately outputs 0% progress so that the user
 * gets feedback as soon as possible.
 */
void progress_init(struct fwup_progress *progress,
                   int progress_low,
                   int progress_high)
{
    progress->last_reported = -1;
    progress->total_units = 0;
    progress->current_units = 0;
    progress->start_time = 0;
    progress->low = progress_low;
    progress->range = progress_high - progress_low;

    output_progress(progress, progress_low);
}

/**
 * @brief Call this to report progress.
 *
 * @param progress the progress info
 * @param units the number of "progress" units to increment
 */
void progress_report(struct fwup_progress *progress, int units)
{
    // Start the timer once we start for real
    if (fwup_progress_mode == PROGRESS_MODE_NORMAL &&
            progress->start_time == 0 &&
            progress->total_units > 0)
        progress->start_time = current_time_ms();

    progress->current_units += units;
    assert(progress->current_units <= progress->total_units);

    int to_report = progress->low;
    if (progress->total_units) {
        int amt = (int) (progress->current_units * progress->range / progress->total_units);

        // Don't report "100%" until the very, very end just in case something takes
        // longer than expected in the code after all progress units have been reported.
        if (amt >= progress->range)
            amt = progress->range - 1;
        to_report += amt;
    }

    output_progress(progress, to_report);
}

/**
 * @brief Call this when the operation is 100% complete
 *
 * @param progress the progress info
 */
void progress_report_complete(struct fwup_progress *progress)
{
    // Force 100%
    output_progress(progress, progress->low + progress->range);

    switch (fwup_progress_mode) {
    case PROGRESS_MODE_NORMAL:
        if (progress->start_time) {
            int elapsed = current_time_ms() - progress->start_time;
            printf("\nElapsed time: %d.%03ds\n",
               elapsed / 1000,
               elapsed % 1000);
        }
        break;

    case PROGRESS_MODE_NUMERIC:
    case PROGRESS_MODE_FRAMING:
    case PROGRESS_MODE_OFF:
    default:
        break;
    }
}

