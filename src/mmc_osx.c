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

#if __APPLE__
#include "mmc.h"
#include "util.h"

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>

#include <sys/socket.h>
#include <sys/param.h>

// DiskArbitration API session
static DASessionRef da_session;

/**
 * Initialize mmc support
 */
void mmc_init()
{
    da_session = DASessionCreate(kCFAllocatorDefault);
    DASessionScheduleWithRunLoop(da_session, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
}

/**
 * Free memory and resources for working with MMC devices
 */
void mmc_finalize()
{
    CFRelease(da_session);
}

static const char *mmc_path_to_bsdname(const char *mmc_device)
{
    // mmc_devices have two forms: "/dev/diskX" and "/dev/rdiskX"
    // DADiskCreateFromBSDName wants "diskX"

    // Check for enough characters in the device name for either form.
    if (strlen(mmc_device) < 10)
        return NULL;

    const char *bsdname = NULL;
    if (memcmp(mmc_device, "/dev/disk", 9) == 0)
        bsdname = &mmc_device[5];
    else if (memcmp(mmc_device, "/dev/rdisk", 10) == 0)
        bsdname = &mmc_device[6];
    else
        return NULL;

    // Check that we're passed a whole disk as the mmc_device (e.g. only numbers between disk and the end of string)
    int offset = strspn(&bsdname[4], "0123456789");
    if (bsdname[4 + offset] != '\0')
        return NULL;

    return bsdname;
}

static DADiskRef mmc_device_to_diskref(const char *mmc_device)
{
    const char *bsdname = mmc_path_to_bsdname(mmc_device);
    if (bsdname == NULL)
        return NULL;

    // Let the Disk Arbitration API perform any additional checks and return the DADiskRef
    return DADiskCreateFromBSDName(kCFAllocatorDefault, da_session, bsdname);
}

struct scan_context
{
    struct mmc_device *devices;
    int max_devices;
    int count;
};

static void scan_disk_appeared_cb(DADiskRef disk, void *c)
{
    struct scan_context *context = (struct scan_context *) c;
    int ix = context->count;
    if (ix < context->max_devices) {
        sprintf(context->devices[ix].path, "/dev/r%s", DADiskGetBSDName(disk));

        CFDictionaryRef info = DADiskCopyDescription(disk);
        CFNumberRef cfsize = CFDictionaryGetValue(info, kDADiskDescriptionMediaSizeKey);
        int64_t size;
        CFNumberGetValue(cfsize, kCFNumberSInt64Type, &size);
        context->devices[ix].size = size;
        CFRelease(info);

        context->count++;
    }
}

static void timeout_cb(CFRunLoopTimerRef timer, void *context)
{
    (void) timer;

    bool *timed_out = (bool *) context;
    *timed_out = true;

    CFRunLoopStop(CFRunLoopGetCurrent());
}

static int run_loop_for_time(double duration)
{
    bool timed_out = false;
    CFRunLoopTimerContext timer_context = { 0, &timed_out, NULL, NULL, NULL };
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + duration, 0.0, 0, 0, timeout_cb, &timer_context);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);

    CFRunLoopRun();
    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);
    CFRelease(timer);

    return timed_out ? -1 : 0;
}

int mmc_scan_for_devices(struct mmc_device *devices, int max_devices)
{
    // Only look for removable media
    CFMutableDictionaryRef toMatch =
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(toMatch, kDADiskDescriptionMediaWholeKey, kCFBooleanTrue);
    CFDictionaryAddValue(toMatch, kDADiskDescriptionMediaRemovableKey, kCFBooleanTrue);

    struct scan_context context;
    context.devices = devices;
    context.max_devices = max_devices;
    context.count = 0;
    DARegisterDiskAppearedCallback(da_session, toMatch, scan_disk_appeared_cb, &context);


    // Scan for removable media for 100 ms
    // NOTE: It's not clear how long the event loop has to run. Ideally, it would
    // terminate after all devices have been found, but I don't know how to do that.
    run_loop_for_time(0.1);

    return context.count;
}

/**
 * @brief Run authopen to acquire a file descriptor to the mmc device
 *
 * Like Linux, OSX does not allow processes to read and write devices as
 * normal users. OSX provides a utility called authopen that can ask the
 * user for permission to access a file.
 *
 * @param pathname the full path to the device
 * @return a descriptor or -1 on error
 */
static int authopen_fd(char * const pathname)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0)
        err(EXIT_FAILURE, "Can't create socketpair");

    pid_t pid = fork();
    if (pid == 0) {
        // child
        int devnull = open("/dev/null", O_RDWR);
        if (devnull < 0)
            err(EXIT_FAILURE, "/dev/null");

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        if (dup2(devnull, STDIN_FILENO) < 0)
            err(EXIT_FAILURE, "dup2 devnull");
        if (dup2(sockets[1], STDOUT_FILENO) < 0)
            err(EXIT_FAILURE, "dup2 pipe");
        close(devnull);

        char permissions[16];
        sprintf(permissions, "%d", O_RDWR);
        char * const exec_argv[] = { "/usr/libexec/authopen",
                              "-stdoutpipe",
                              "-o",
                              permissions,
                              pathname,
                              0 };
        execvp(exec_argv[0], exec_argv);

        // Not supposed to reach here.
        err(EXIT_FAILURE, "execvp failed");
    } else {
        // parent
        close(sockets[1]); // No writes to the pipe

        // Receive the authorized file descriptor from authopen
        char buffer[sizeof(struct cmsghdr) + sizeof(int)];
        struct iovec io_vec[1];
        io_vec[0].iov_base = buffer;
        io_vec[0].iov_len = sizeof(buffer);

        struct msghdr message;
        memset(&message, 0, sizeof(message));
        message.msg_iov = io_vec;
        message.msg_iovlen = 1;

        char cmsg_socket[CMSG_SPACE(sizeof(int))];
        message.msg_control = cmsg_socket;
        message.msg_controllen = sizeof(cmsg_socket);

        int fd = -1;
        for (;;) {
            ssize_t size = recvmsg(sockets[0], &message, 0);
            if (size > 0) {
                struct cmsghdr* cmsg_socket_header = CMSG_FIRSTHDR(&message);
                if (cmsg_socket_header &&
                        cmsg_socket_header->cmsg_level == SOL_SOCKET &&
                        cmsg_socket_header->cmsg_type == SCM_RIGHTS) {
                    // Got file descriptor
                    memcpy(&fd, CMSG_DATA(cmsg_socket_header), sizeof(fd));
                    break;
                }
            } else if (errno != EINTR) {
                // Any other cause
                break;
            }
        }

        // No more reads from the pipe.
        close(sockets[0]);

        return fd;
    }
}

/**
 * Return a file handle to the specified path for mmc devices
 *
 * @param mmc_path
 * @return
 */
int mmc_open(const char *mmc_path)
{
    const char *bsdname = mmc_path_to_bsdname(mmc_path);
    if (bsdname == NULL)
        return -1;

    // always operate on the raw device
    char raw_path[16];
    sprintf(raw_path, "/dev/r%s", bsdname);

    // Use authopen to get permissions to the device
    return authopen_fd(raw_path);
}

struct disk_op_context
{
    const char *operation;
    bool succeeded;
};

static void disk_op_done_cb(DADiskRef disk, DADissenterRef dissenter, void *c)
{
    (void) disk;

    struct disk_op_context *context = (struct disk_op_context *) c;
    if (dissenter) {
        CFStringRef what = DADissenterGetStatusString(dissenter);
        warnx("%s failed: 0x%x (%d) %s)",
                context->operation,
                DADissenterGetStatus(dissenter),
                DADissenterGetStatus(dissenter),
                CFStringGetCStringPtr(what, kCFStringEncodingMacRoman));
        context->succeeded = false;
    } else {
        context->succeeded = true;
    }

    CFRunLoopStop(CFRunLoopGetCurrent());
}

int mmc_umount_all(const char *mmc_device)
{
    DADiskRef disk = mmc_device_to_diskref(mmc_device);
    int rc = -1;
    if (disk) {
        struct disk_op_context context;
        context.operation = "unmount";
        DADiskUnmount(disk, kDADiskUnmountOptionWhole, disk_op_done_cb, &context);

        // Wait for a while since unmounting sometimes takes time.
        if (run_loop_for_time(10) < 0)
            warnx("unmount timed out");

        if (context.succeeded)
            rc = 0;
        CFRelease(disk);
    }
    return rc;
}

int mmc_eject(const char *mmc_device)
{
    DADiskRef disk = mmc_device_to_diskref(mmc_device);
    int rc = -1;
    if (disk) {
        struct disk_op_context context;
        context.operation = "eject";
        DADiskEject(disk, kDADiskEjectOptionDefault, disk_op_done_cb,  &context);

        if (run_loop_for_time(10) < 0)
            warnx("unmount timed out");

        if (context.succeeded)
            rc = 0;

        CFRelease(disk);
    }
    return rc;
}
#endif // __APPLE__
