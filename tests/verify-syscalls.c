
// verify-syscalls intercepts syscalls from fwup to
// check that they obey constraints on reads and writes. It works
// in a similar way to strace(1) except that it will exit
// immediately when a bad call has been issued. Example
// constraints are:
//
//   1. Reads and writes are always block-sized amounts
//   2. Reads and writes are always on block-sized offsets
//   3. Reads and writes specify buffers on pages boundaries
//
// These are important since fwup
// issues raw, unbuffered reads and writes to memory cards and
// must follow OS constraints or the calls will fail. The reason
// is does this is for performance and predictability of
// programming operations.
//
// In normal use, verify-syscalls is called with the commandline
// parameters of the fwup command that needs verification. The
// path to fwup should be specified by the `VERIFY_SYSCALLS_CMD`
// environment variable.
//
// Environment variables:
//  VERIFY_SYSCALLS_CMD - which command to run
//  VERIFY_SYSCALLS_CHECKPATH - which file to monitor (defaults to fwup.img)

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/reg.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>

static pid_t child = -1;
static int check_fd = -1;
static off_t check_offset = -1;
static const char *check_pathname = "fwup.img";

static int check_block_size = 512;
static int open_call_count = 0;
static int write_call_count = 0;
static int write_call_bytes = 0;
static int read_call_count = 0;
static int read_call_bytes = 0;

static void exec_and_trace(char * const argv[])
{
    child = fork();
    if(child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(argv[0], argv);
        err(EXIT_FAILURE, "execl");
    }
}

static long get_syscall_params(int param_count, long *params)
{
    static const int regs[] = { RDI, RSI, RDX, R10, R8, R9 };
    for (int i = 0; i < param_count; i++) {
        params[i] = ptrace(PTRACE_PEEKUSER,
                           child, sizeof(long) * regs[i],
                           NULL);
        if (errno)
            err(EXIT_FAILURE, "ptrace(PTRACE_PEEKUSER, %d)", regs[i]);
    }

    long rc = ptrace(PTRACE_PEEKUSER,
                       child, sizeof(long) * RAX,
                       NULL);
    if (errno)
        err(EXIT_FAILURE, "ptrace(PTRACE_PEEKUSER, RAX)");
    return rc;
}

static void get_syscall_str(long addr, char *buffer, size_t len)
{
    for (size_t i = 0; i < len; i += sizeof(long)) {
        long data = ptrace(PTRACE_PEEKDATA,
                                  child, addr + i,
                                  NULL);

        size_t to_copy = (i + sizeof(long) > len) ? len - i : sizeof(long);
        memcpy(&buffer[i], &data, to_copy);

        // Check for null terminator
        for (size_t j = 0; j < sizeof(long); j++) {
            if (buffer[i + j] == '\0')
                return;
        }
    }

    // Force null terminator
    buffer[len - 1] = '\0';
}

static void assert_size_valid(const char *what, size_t size)
{
    size_t leftovers = size % check_block_size;
    if (leftovers > 0)
        errx(EXIT_FAILURE, "not block size (%d) number of bytes for '%s': %ld bytes", check_block_size, what, size);
}

static void assert_offset_valid(const char *what, off_t offset)
{
    if (offset < 0)
        errx(EXIT_FAILURE, "Negative offset passed to '%s': %ld", what, offset);

    size_t leftovers = offset % check_block_size;
    if (leftovers > 0)
        errx(EXIT_FAILURE, "Not block size (%d) offset passed to '%s': %ld", check_block_size, what, offset);
}

static void assert_buffer_valid(const char *what, long addr)
{
    long leftovers = addr & ~(4096 - 1);
    if (leftovers > 0)
        warnx("Buffer passed to '%s' is not on a page boundary: 0x%08lx", what, addr);
}

static void handle_read()
{
    // sys_read(int fd, char *buf, size_t count)
    long params[3];
    long rc = get_syscall_params(3, params);

    if (params[0] == check_fd) {
        //fprintf(stderr, "read(%ld, %ld)\n", params[0], params[2]);

        read_call_count++;
        assert_size_valid("read", params[2]);
        assert_offset_valid("read", check_offset);
        assert_buffer_valid("read", params[1]);

        if (rc >= 0) {
            read_call_bytes += rc;
            assert_size_valid("read result", rc);

            check_offset += rc;
        }
    }
}

static void handle_write()
{
    // sys_write(unsigned int fd, const char *buf, size_t count)
    long params[3];
    long rc = get_syscall_params(3, params);
    if (params[0] == check_fd) {
        //fprintf(stderr, "write(%ld, %ld)\n", params[0], params[2]);
        write_call_count++;

        assert_size_valid("write", params[2]);
        assert_offset_valid("write", check_offset);
        assert_buffer_valid("write", params[1]);

        if (rc >= 0) {
            write_call_bytes += rc;
            assert_size_valid("write result", rc);

            check_offset += rc;
        }
    }
}

static void handle_open()
{
    // fd = sys_open(const char *filename, int flags, int mode)
    long params[3];
    long rc = get_syscall_params(3, params);

    char path[PATH_MAX];
    get_syscall_str(params[0], path, sizeof(path));

    if (rc >= 0) {
        // Check if the process successfully opened the file of interest.
        // (match on suffix)
        size_t check_pathname_len = strlen(check_pathname);
        size_t path_len = strlen(path);
        if (path_len >= check_pathname_len &&
                strstr(path + path_len - check_pathname_len, check_pathname) != 0) {
            //fprintf(stderr, "open(\"%s\") -> %ld\n", path, rc);
            check_fd = rc;
            check_offset = 0;

            open_call_count++;
            if (open_call_count != 1)
                errx(EXIT_FAILURE, "open was called on '%s' more than once?", check_pathname);
        }
    }
}

static void handle_close()
{
    // sys_close(int flags)
    long params[1];
    get_syscall_params(1, params);

    if (check_fd == params[0]) {
        //fprintf(stderr, "close(%ld)\n", params[0]);
        check_fd = -1;
    }
}

static void handle_lseek()
{
    // sys_lseek(int fd, off_t offset, unsigned int origin)
    long params[3];
    long rc = get_syscall_params(3, params);

    if (check_fd == params[0]) {
        //fprintf(stderr, "lseek(%ld, %ld, %ld)\n", params[0], params[1], params[2]);
        // Save the new file descriptor position
        check_offset = rc;
    }
}

static void handle_pread64()
{
    // sys_pread64(int fd, char *buf, size_t count, off_t pos)
    long params[4];
    long rc = get_syscall_params(4, params);

    if (check_fd == params[0]) {
        // Check pread64 parameters
        //fprintf(stderr, "pread64(%ld, %ld, %ld)\n", params[0], params[2], params[3]);

        read_call_count++;

        assert_size_valid("pread64", params[2]);
        assert_offset_valid("pread64", params[3]);
        assert_buffer_valid("pread64", params[1]);
        if (rc >= 0) {
            read_call_bytes += rc;
            assert_size_valid("pread64 result", rc);
        }
    }
}

static void handle_pwrite64()
{
    // sys_pwrite64(int fd, const char *buf, size_t count, off_t pos)
    long params[4];
    long rc = get_syscall_params(4, params);

    if (check_fd == params[0]) {
        // Check pwrite64 parameters
        //fprintf(stderr, "pwrite64(%ld, %ld, %ld)\n", params[0], params[2], params[3]);
        write_call_count++;

        assert_size_valid("pwrite64", params[2]);
        assert_offset_valid("pwrite64", params[3]);
        assert_buffer_valid("pwrite64", params[1]);
        if (rc >= 0) {
            write_call_bytes += rc;
            assert_size_valid("pwrite64 result", rc);

            check_offset += rc;
        }
    }
}

static void handle_readv()
{
    // sys_readv(int fd, const struct iovec *vec, unsigned long vlen)
    errx(EXIT_FAILURE, "sys_readv unimplemented");
}

static void handle_writev()
{
    // sys_writev(int fd, const struct iovec *vec, unsigned long vlen)
    errx(EXIT_FAILURE, "sys_writev unimplemented");
}

int main(int argc, char *argv[])
{
    char *program_name = getenv("VERIFY_SYSCALLS_CMD");
    if (program_name != NULL) {
        argv[0] = program_name;
        exec_and_trace(argv);
    } else {
        if (argc == 1)
            errx(EXIT_FAILURE, "Expecting command to run");

        exec_and_trace(&argv[1]);
    }

    char *override_path = getenv("VERIFY_SYSCALLS_CHECKPATH");
    if (override_path)
        check_pathname = override_path;

    // Wait for first signal
    int status;
    wait(&status);
    if(WIFEXITED(status))
        errx(EXIT_FAILURE, "Process exited before tracing could start");

    bool enter_syscall = false;
    for (;;) {
        ptrace(PTRACE_SYSCALL, child, NULL, NULL);
        wait(&status);
        if(WIFEXITED(status))
            break;

        // Not sure how to detect which side of the syscall we're on other
        // than doing this.
        enter_syscall = !enter_syscall;

        // Only handling the exit out of the syscall
        if (enter_syscall)
            continue;

        long orig_eax = ptrace(PTRACE_PEEKUSER,
                               child, 8 * ORIG_RAX,
                               NULL);
        if (errno)
            err(EXIT_FAILURE, "ptrace(PTRACE_PEEKUSER, ORIG_RAX)");
        switch (orig_eax) {
        case SYS_close:
            handle_close();
            break;
        case SYS_open:
            handle_open();
            break;
        case SYS_lseek:
            handle_lseek();
            break;
        case SYS_pread64:
            handle_pread64();
            break;
        case SYS_pwrite64:
            handle_pwrite64();
            break;
        case SYS_read:
            handle_read();
            break;
        case SYS_readv:
            handle_readv();
            break;
        case SYS_write:
            handle_write();
            break;
        case SYS_writev:
            handle_writev();
            break;

        default:
            // Ignore
            break;
        }
    }

    if (open_call_count == 0)
        errx(EXIT_FAILURE, "open was never called on '%s'", check_pathname);

    // Dump statistics
    fprintf(stderr, "verify-syscalls report\n");
    fprintf(stderr, "Calls to write(): %d\n", write_call_count);
    if (write_call_count > 0) {
        fprintf(stderr, "Bytes written: %d\n", write_call_bytes);
        fprintf(stderr, "Averate bytes written/call: %f bytes/call\n", ((double) write_call_bytes) / write_call_count);
    }

    fprintf(stderr, "Calls to read(): %d\n", read_call_count);
    if (read_call_count > 0) {
        fprintf(stderr, "Bytes read: %d\n", read_call_bytes);
        fprintf(stderr, "Averate bytes read/call: %f bytes/call\n", ((double) read_call_bytes) / read_call_count);
    }

    // Pass on the exit status.
    return WEXITSTATUS(status);
}
