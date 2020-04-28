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

#include "progress.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

// Elapsed time measurement maxes out at 2^31 ms = 24 days
// NOTE: Windows builds on Travis report clock_gettime but fail. Windows
//       builds on my laptop don't report clock_gettime and succeed.
//       Therefore, disable support on Windows.
#if defined(HAVE_CLOCK_GETTIME) && !defined(_WIN32) && !defined(__CYGWIN__)
#include <time.h>
static int current_time_ms()
{
    struct timespec tp;

    // Prefer the monotonic clock, but fail back to the
    // realtime clock.
    if (clock_gettime(CLOCK_MONOTONIC, &tp) < 0 &&
        clock_gettime(CLOCK_REALTIME, &tp) < 0)
        fwup_err(EXIT_FAILURE, "clock_gettime failed");

    return (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);
}
#else
#include <sys/time.h>
// gettimeofday isn't monotonic, but it's better than nothing.
static int current_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}
#endif

static void draw_progress_bar(struct fwup_progress *progress, int percent)
{
    // The progress bar looks something like this:
    // |=====================                             | 43% (23 / 52) MB
    //
    // The #defines below calculate the number of equal sizes to use based
    // on the terminal width. Since we can't query the terminal width, we have
    // to guess something reasonable. 80 characters had previously been assumed,
    // but it's quite common for people to run their upgrades in thin windows
    // in tmux, so set lower. 50 equal signs works for 80 characters, so lower
    // this number for thinner terminals.

#define PROGRESS_BITS     36
#define PROGRESS_BITS_STR "36"

    static const char fifty_equals[] = "==================================================";

    if (progress->total_units <= 0) {
        printf("\r%3d%% [%-" PROGRESS_BITS_STR ".*s]",
               percent,
               percent * PROGRESS_BITS / 100, fifty_equals);
    } else {
        off_t read_units = find_natural_units(progress->input_bytes);
        off_t written_units = find_natural_units(progress->current_units);
        printf("\r%3d%% [%-" PROGRESS_BITS_STR ".*s] %.2f %s in / %.2f %s out     \b\b\b\b\b",
               percent,
               percent * PROGRESS_BITS / 100, fifty_equals,
               ((double) progress->input_bytes) / read_units,
               units_to_string(read_units),
               ((double) progress->current_units) / written_units,
               units_to_string(written_units));
    }
}

static void output_progress(struct fwup_progress *progress, int percent)
{
    if (percent <= progress->last_reported_percent)
        return;

    progress->last_reported_percent = percent;

    switch (fwup_progress_mode) {
    case PROGRESS_MODE_NUMERIC:
        printf("%d\n", percent);
        fflush(stdout);
        break;

    case PROGRESS_MODE_NORMAL:
        draw_progress_bar(progress, percent);
        fflush(stdout);
        break;

    case PROGRESS_MODE_FRAMING:
        fwup_output(FRAMING_TYPE_PROGRESS, percent, "");
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
    progress->last_reported_percent = -1;
    progress->total_units = 0;
    progress->current_units = 0;
    progress->start_time = 0;
    progress->low = progress_low;
    progress->range = progress_high - progress_low;
    progress->input_bytes = 0;

    output_progress(progress, progress_low);
}

/**
 * @brief Call this to report progress.
 *
 * @param progress the progress info
 * @param units the number of "progress" units to increment
 */
void progress_report(struct fwup_progress *progress, uint64_t units)
{
    // Start the timer once we start for real
    if (fwup_progress_mode == PROGRESS_MODE_NORMAL &&
            progress->start_time == 0 &&
            progress->total_units > 0)
        progress->start_time = current_time_ms();

    progress->current_units += units;

    // This should never happen. If it does, it definitely should be fixed, but
    // don't throw an error or warning, since failing an update due to a progress
    // calculation is hard to justify to anyone.
    if (progress->current_units > progress->total_units)
        progress->current_units = progress->total_units;

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
        printf("\nSuccess!\n");
        if (progress->start_time) {
            int elapsed = current_time_ms() - progress->start_time;
            if (elapsed > 60000) {
                int seconds = elapsed / 1000;
                printf("Elapsed time: %d min %02d s\n",
                   seconds / 60,
                   seconds % 60);
            } else {
                printf("Elapsed time: %d.%03d s\n",
                   elapsed / 1000,
                   elapsed % 1000);
            }
        }
        break;

    case PROGRESS_MODE_NUMERIC:
    case PROGRESS_MODE_FRAMING:
    case PROGRESS_MODE_OFF:
    default:
        break;
    }
}

