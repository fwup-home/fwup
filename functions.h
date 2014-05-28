#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <stddef.h>
#include <sys/types.h>

enum fun_context_type {
    FUN_CONTEXT_INIT,
    FUN_CONTEXT_FINISH,
    FUN_CONTEXT_ERROR,
    FUN_CONTEXT_FILE
};

struct fun_context {
    // Context of where the function is called
    enum fun_context_type type;

    // Function name and arguments
    int argc;
    const char **argv;

    // If the context supplies data, this is the expected byte count
    size_t expected_bytecount;

    // If the context supplies data, this function gets it. If read returns 0,
    // no more data is available. If <0, then there's an error.
    ssize_t (*read)(struct fun_context *fctx, void *buffer, size_t len);

    // Callback for reporting progress
    void (*report_progress)(int progress_units);
};

int fun_validate(struct fun_context *fctx);
int fun_run(struct fun_context *fctx);
int fun_calc_progress_units(struct fun_context *fctx, int *progress_units);

#endif // FUNCTIONS_H
