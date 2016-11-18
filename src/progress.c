#include "progress.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief Initialize progress reporting
 *
 * This function also immediately outputs 0% progress so that the user
 * gets feedback as soon as possible.
 */
void progress_init(struct fwup_progress *progress, enum fwup_progress_mode mode)
{
    memset(progress, 0, sizeof(*progress));
    progress->mode = mode;
    progress->last_reported = -1;
    progress_report(progress, 0);
}

/**
 * @brief Call this to report progress.
 *
 * @param progress the progress info
 * @param units the number of "progress" units to increment
 */
void progress_report(struct fwup_progress *progress, int units)
{
    if (progress->mode == PROGRESS_MODE_OFF)
        return;

    progress->current_units += units;
    int percent;
    if (progress->total_units > 0)
        percent = (int) ((100.0 * progress->current_units + 50.0) / progress->total_units);
    else
        percent = 0;

    // Don't report 100% until the very, very end just in case something takes
    // longer than expected in the code after all progress units have been reported.
    if (percent > 99)
        percent = 99;

    if (percent == progress->last_reported)
        return;

    progress->last_reported = percent;

    switch (progress->mode) {
    case PROGRESS_MODE_NUMERIC:
        printf("%d\n", percent);
        break;

    case PROGRESS_MODE_NORMAL:
        printf("\r%3d%%", percent);
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
 * @brief Call this when the operation is 100% complete
 *
 * @param progress the progress info
 */
void progress_report_complete(struct fwup_progress *progress)
{
    switch (progress->mode) {
    case PROGRESS_MODE_NUMERIC:
        printf("100\n");
        break;

    case PROGRESS_MODE_NORMAL:
        printf("\r100%%\n");
        break;

    case PROGRESS_MODE_FRAMING:
        fwup_output(FRAMING_TYPE_PROGRESS, 100, "");
        break;

    case PROGRESS_MODE_OFF:
    default:
        break;
    }
}

