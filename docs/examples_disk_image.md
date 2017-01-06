# Disk image distribution

## Goal

Create a bootable embedded Linux image and use `fwup` to package it.

## Tutorial

This simplest use of `fwup` is to package SDCard images for distribution. For
example, instead of distributing an SDCard image with programming instructions
for `dd(1)` or
[Win32DiskImager](https://sourceforge.net/projects/win32diskimager/), you could
use `fwup`. Granted `fwup` isn't as well-known, images are compressed by default
and it can program SDCards much faster in some cases.

To demonstrate, let's use Buildroot to create a barebones Linux image for an
emulated ARM platform. The Linux image will consist of a Master Boot Record
(MBR), U-Boot bootloader and a Linux root filesystem. The Linux root filesystem
will contain the Linux kernel (in the `/boot` directory and many familiar
userland utilities in `/usr`). If you haven't heard of an MBR, it just tells the
OS where to find filesystems in the image. This image only has one, the Linux
root filesystem partition. This setup is similar to that used on many embedded
boards.

The boot process works as follows:

1. The processor loads a built-in bootloader (sometimes called ROM bootloader).
2. The ROM bootloader checks a set of predefined locations for the next
   bootloader (in this case U-Boot).
3. U-Boot loads the Linux kernel.

Since we'll be emulating a real board with QEMU, the boot process is simpler.
QEMU skips over the ROM bootloader part and we have to tell it how to find
U-Boot via a commandline argument. However, for sake of illustration, we'll set
the image up like it would like on a real system that boots off an SDCard or
eMMC memory.

Let's take a look at the `fwup` configuration file section by section. The
completed config file can be found in `board/vexpress/simple/fwup.conf`.

The first part describes the layout and defines constants for later use:
```
# This configuration file will create an image that
# has an MBR and the following layout:
#
# +----------------------------+
# | MBR                        |
# +----------------------------+
# | U-Boot (384K)              |
# +----------------------------+
# | U-Boot environment (8K)    |
# +----------------------------+
# | p0: Rootfs (ext4)          |
# +----------------------------+
# | p1: Unused                 |
# +----------------------------+
# | p2: Unused                 |
# +----------------------------+
# | p3: Unused                 |
# +----------------------------+
#

define(UBOOT_OFFSET, 256)
define(UBOOT_COUNT, 768)
define(UBOOT_ENV_OFFSET, 1024)
define(UBOOT_ENV_COUNT, 8)
define(ROOTFS_PART_OFFSET, 2048)
define(ROOTFS_PART_COUNT, 524288)
```
In this layout, the MBR is stored in the first block. This is always the case
when using MBR-based partitioning. The U-Boot image and its environment are
stored next in the image and neither exists in a partition. We'll pretend that
the ROM bootloader knows to load and run U-Boot from that location. The
`define(UBOOT_OFFSET, 256)` defines a constant for later with the block offset
of U-Boot. `fwup` is similar in this way to `dd(1)` in its use of 512-byte
blocks for offsets and counts.  Block 256 corresponds to the location at 128 KiB
from the beginning. The U-Boot environment location is hardcoded into the U-Boot
binary. (Since the default location that upstream U-Boot uses to store its
environment on the emulated board wasn't interesting, I changed it in the
patches in `board/vexpress/common/u-boot/patches`.)

Finally, the root filesystem is located at block 2048 or 1 MiB from the start.
We'll list the root filesystem offset/count in the MBR later so that Linux can
refer to it as `/dev/mmcblk0p1`.

The next sections of the `fwup` configuration define where to find the files
when creating the firmware archive:
```
file-resource u-boot {
    host-path = "${BINARIES_DIR}/u-boot"
}
file-resource uboot-env.bin {
    host-path = "${BINARIES_DIR}/uboot-env.bin"
}
file-resource rootfs.img {
    host-path = "${BINARIES_DIR}/rootfs.ext2"
}
```
In `fwup` every binary blob of data is called a "resource" in the firmware
archive. The above defines three resources with the names, `u-boot`,
`uboot-env.bin`, and `rootfs.img`. Resource names don't have to match the names
on the filesystem. The `host-path` specifies where to find the files. It can be
set using environment variables. In this case, `$BINARIES_DIR` is defined by
Buildroot to point to the directory containing its build products.

The next section defines the MBR:
```
mbr mbr_a {
    partition 0 {
        block-offset = ${ROOTFS_PART_OFFSET}
        block-count = ${ROOTFS_PART_COUNT}
        type = 0x83 # Linux
    }
    # partition 1 is unused
    # partition 2 is unused
    # partition 3 is unused
}
```
`fwup` supports defining multiple MBRs and picking one based on how it's
invoked. In this case, the MBR is named `mbr_a`. Only the first partition is
defined. See [Wikipedia](https://en.wikipedia.org/wiki/Partition_type) for a
list of partition types. While `fwup` doesn't care what's listed, U-Boot and
Linux use this to figure out which filesystem is being used.

Finally, the `fwup` config file has a list of tasks. This configuration only has
one task, but most real configurations will have more than one. By convention,
the task that programs the whole Flash image like would be done in manufacturing
or for a first-time program is named `complete`:

```
task complete {
    on-init {
        mbr_write(mbr_a)
    }
    on-resource u-boot {
        raw_write(${UBOOT_OFFSET})
    }
    on-resource uboot-env.bin {
        raw_write(${UBOOT_ENV_OFFSET})
    }
    on-resource rootfs.img {
        raw_write(${ROOTFS_PART_OFFSET})
    }
}
```
Tasks reveal how `fwup` works at a low level. Basically, `fwup` makes one pass
over the archive and as resources are seen, actions are performed. The
single-pass processing of archives allows firmware updates to be streamed to
destination devices. This isn't a big deal now, but it will be later. The mental
model to have in your mind when reading task definitions is to read them as a
list of event handlers. Briefly, the above definition writes the MBR on
initialization and then writes each resource to its location on the SDCard.

Now that we've gone over the `fwup` configuration file, lets create and image
and try it out. For the impatient, you may want to skim since the builds take a
while.

## System prep

If you're using Ubuntu, you may need to install some packages to make Buildroot
and these examples work. This should be sufficient:

    $ sudo apt-get install git g++ libncurses5-dev bc make unzip zip qemu-system-arm

Buildroot is going to download quite a few files. If you're already a Buildroot
user and have a directory that you use system-wide to cache these files, set the
`BUILDROOT_DL_DIR` environment variable and the scripts run by the examples will
use it.

## Building

All builds are run in directories that are separate from the source tree. This
allows multiple configurations to be tried simultaneously and keeps the source
tree clean. This will come in handy in future examples. If you're running low on
disk space, though, you'll want to erase the builds since they're large (this
one is ~3.5GB).

Assuming that you've cloned this project already, create a build directory by
running `./create_build.sh`. For this example, run:

    $ ./create_build.sh configs/vexpress_simple_defconfig

Then build the firmware:

    $ cd o/vexpress_simple
    $ make

It can take some time to download and build everything so you may need to be
patient. The build products can be found in the `images` directory.

```
$ ls -las images/
total 21888
   4 drwxr-xr-x 2 fhunleth fhunleth      4096 Nov 30 09:22 .
   4 drwxrwxr-x 6 fhunleth fhunleth      4096 Nov 30 09:18 ..
7448 -rw-r--r-- 1 fhunleth fhunleth 268435456 Nov 30 09:22 rootfs.ext2
   0 lrwxrwxrwx 1 fhunleth fhunleth        11 Nov 30 09:22 rootfs.ext4 ->
rootfs.ext2
 200 -rwxr-xr-x 1 fhunleth fhunleth    264944 Nov 30 09:22 u-boot
   8 -rw-r----- 1 fhunleth fhunleth      8192 Nov 30 09:21 uboot-env.bin
4576 -rw-r--r-- 1 fhunleth fhunleth   4683444 Nov 30 09:22 vexpress.fw
7724 -rw-r--r-- 1 fhunleth fhunleth 269484032 Nov 30 09:22 vexpress.img
  16 -rw-r--r-- 1 fhunleth fhunleth     14708 Nov 30 09:22 vexpress-v2p-ca9.dtb
1908 -rw-r--r-- 1 fhunleth fhunleth   1950608 Nov 30 09:22 zImage
```

The `vexpress.fw` file is the firmware package created by `fwup`. Notice that it
is quite small compared to the root filesystem, `roofs.ext2`. The reason this is
the case is that the root filesystem was formatted to be 256 MB but it has a lot
of freespace. It would be possible to shrink the filesystem and then expand it
on first use, but that's not always desirable in real scenarios. The other
important file is `vexpress.img`. This is the raw image created by `fwup` that
would be distributed if you wanted to use `dd(1)` or Win32DiskImager.

As a side note, the files above don't actually take up that much space on disk.
The `rootfs.ext2` and `vexpress.img` contain large blocks of unused space
(implicitly set to 0s) that aren't physically stored by the filesystem. See
[sparse file](https://en.wikipedia.org/wiki/Sparse_file) for more information.

## Trying it out

Since we're mostly interested in `fwup`, let's program the image file to an
SDCard or USB flash drive to see it in action. Make sure that you use a device
that you don't care gets erased. Here's an example run on my system:

    $ cd o/vexpress_simple
    $ sudo host/usr/bin/fwup images/vexpress.fw
    Use 7.22 GiB memory card found at /dev/sdc? [y/N] y
    100%
    Elapsed time: 31.584s

The `host/usr/bin/fwup` is the version of `fwup` built for the host by
Buildroot.  It is fine to use one that you've installed on your system. On a
real project, the Buildroot provided version can be useful for guaranting that a
particular version of `fwup` is always used in production.

The commandline invocation above hides many commandline parameters for
simplicity.  It is identical to running:

    $ sudo host/usr/bin/fwup --apply -i images/vexpress.fw --task complete

It's also possible to create the raw image for `dd(1)` with `fwup`:

    $ $ host/usr/bin/fwup images/vexpress.fw -d myimage.bin
    100%
    Elapsed time: 0.107s

That image can also be written to the SDCard or USB Flash drive. BE SURE TO
UPDATE THE PARAMETERS BEFORE TRYING THIS!!!

    $ sudo dd if=myimage.bin of=/dev/sdc bs=1M
    257+0 records in
    257+0 records out
    269484032 bytes (269 MB, 257 MiB) copied, 45.0175 s, 6.0 MB/s

The difference in time is due to `fwup` knowing more about the actual image and
not having to write to unused locations.

While it's not important for this example, future examples will run `fwup` in
QEMU.  However, we just waited all that time to build the image, so here's how
to log into it:

    $ cd o/vexpress_simple
    $ ../../run-qemu.sh

Once the image boots, you can also ssh into the image via port 10022:

    $ ssh -p 10022 root@localhost

Check out the `run-qemu.sh` shell script for more information on how QEMU is
invoked.  Note that you can't CTRL-C out of QEMU. To quit, log in and run
"poweroff". The other option is to kill qemu-system-arm from another terminal.
