# Overview
[![Build Status](https://travis-ci.org/fhunleth/fwup.png)](https://travis-ci.org/fhunleth/fwup)

The `fwup` utility is a configurable image-based firmware update utility
for embedded Linux-based systems. It has two modes of operation. The first
mode creates compressed archives containing
root file system images, bootloaders, and other image material. These can be
distributed via websites, email or update servers. The second mode applies
the firmware images in a robust and repeatable way. The utility has the following
features:

  1. Uses standard ZIP archives to make debugging and transmission simple.

  2. Simple, but flexible configuration language to enable firmware
  updates on various platforms and firmware update policies.

  3. Streaming firmware update processing to simplify target storage requirements.

  4. Multiple firmware update task options per archive so that one
  archive can upgrade varying target configurations

  5. Basic disk partitioning and FAT filesystem manipulation

  6. Human and machine readable progress.

  7. Automatic detection of MMC and SDCards. This option queries the
  user before writing anything by default to avoid accidental
  overwrites.

  8. Firmware archive digital signature creation and verification (Not implemented yet)

# Building

You'll need [libconfuse](http://www.nongnu.org/confuse/) and
[libarchive](http://libarchive.org/) installed to build. On Ubuntu and Debian,
you can run:

    sudo apt-get install libarchive-dev libconfuse-dev

Once that completes, clone or download the source code and run the following:

    ./autogen.sh
    ./configure
    make
    make install

# Invoking

```
Usage: fwup [options]
  -a   Apply the firmware update
  -c   Create the firmware update
  -d <Device file for the memory card>
  -f <fwupdate.conf> Specify the firmware update configuration file
  -i <input.fw> Specify the input firmware update file (Use - for stdin)
  -l   List the available tasks in a firmware update
  -m   Print metadata in the firmware update
  -n   Report numeric progress
  -o <output.fw> Specify the output file when creating an update (Use - for stdout)
  -q   Quiet
  -t <task> Task to apply within the firmware update package
  -v   Print version and exit
  -y   Accept automatically found memory card when applying a firmware update

Examples:

Create a firmware update archive:

  $ fwup -c -f fwupdate.conf -o myfirmware.fw

Apply the firmware update to /dev/sdc and specify the 'upgrade' task:

  $ fwup -a -d /dev/sdc -i myfirmware.fw -t upgrade

```

# Configuration file format

`fwup` uses the Unix configuration library, [libconfuse](http://www.nongnu.org/confuse/),
so its configuration has some similarities to other programs. The configuration file is
used to create firmware archives. During creation, `fwup` embeds a processed version of the configuration file into
the archive that has been stripped of comments, has had all variables resolved, and has
some additional useful metadata added. Configuration files are
organized into scoped blocks and options are set using a `key = value` syntax.

## Environment variables

For integration into build systems and other scripts, `fwup` performs environment variable
substitution inside of the configuration files. Keep in mind that environment variables are
resolved on the host during firmware update creation. Environment variables are referenced as
follows:

    key = ${ANY_ENVIRONMENT_VARIABLE}

It is possible to provide default values for environment variables using the `:-` symbol:

    key = ${ANY_ENVIRONMENT_VARIABLE:-adefault}

Inside configuration files, it can be useful to define constants that are used throughout
the file. All constants are stored as environment variables. By default, definitions do
not overwrite environment variables with the same name:

    define(MY_CONSTANT, 5)

To define a constant that does overrides the environment, use `define!`:

    define!(MY_CONSTANT, "Can't override this")


## Global scope

At the global scope, the following options are available:

Key                  | Description
---------------------|------------
require-fwup-version | Require a minimum version of fwup to apply this update
meta-product         | product name
meta-description     | Description of product or firmware update
meta-version         | Firmware update version
meta-author          | Author or company behind the update
meta-platform        | Platform that this update runs on (e.g., rpi or bbb)
meta-architecture    | Platform architectures (e.g., arm)
meta-creation-date   | Timestamp when the update was created (automatically added)

After setting the above options, it is necessary to create scopes for other options. The
currently available scopes are:

Scope                | Description
---------------------|------------
file-resource        | Defines a reference to a file that should be included in the archive
mbr                  | Defines the master boot record contents on the destination
task                 | Defines a firmware update task (referenced using -t from the command line)

## file-resource

A `file-resource` specifies a file on the host that should be included in the archive. Each
`file-resource` should be given a unique name so that it can be referred to by other parts of
the update configuration. The `fwup` will automatically record the length and SHA-256 of the
file in the archive. These fields are used internally to compute progress and verify the contents
of the archive. A typical `file-resource` section looks like this:

```
file-resource zImage {
        host-path = "output/images/zImage"
}
```

## mbr

A `mbr` section specifies the contents of the Master Boot Record on the destination media. This
section contains the partition table that's read by Linux and the bootloaders for finding the
file systems that exist on the media. In comparison to a tool like `fdisk`, `fwup` only supports
simplistic partition setup, but this is sufficient for many devices. Tools such as `fdisk` can be
used to determine the block offsets and sizes of partitions for the configuration file. Offsets
and sizes are given in 512 byte blocks. Here's a potential mbr definition:

```
mbr mbr-a {
        bootstrap-code-path = "path/to/bootstrap-data"

        partition 0 {
                block-offset = ${BOOT_PART_OFFSET}
                block-count = ${BOOT_PART_COUNT}
                type = 0x1 # FAT12
        }
        partition 1 {
                block-offset = ${ROOTFS_A_PART_OFFSET}
                block-count = ${ROOTFS_A_PART_COUNT}
                type = 0x83 # Linux
        }
        partition 2 {
                block-offset = ${ROOTFS_B_PART_OFFSET}
                block-count = ${ROOTFS_B_PART_COUNT}
                type = 0x83 # Linux
        }
        partition 3 {
                block-offset = ${APP_PART_OFFSET}
                block-count = ${APP_PART_COUNT}
                type = 0x83 # Linux
        }
}
```

## task

The `task` section specifies a firmware update task. These sections are the main part of the
firmware update archive since they describe the conditions upon which an update is applied
and the steps to apply the update. Each `task` section must have a unique name, but when searching
for a task, the firmware update tool only does a prefix match. This lets you define multiple tasks
that can be evaluated based on conditions on the target hardware. The first matching task is the
one that gets applied. This can
be useful if the upgrade process is different based on the version of firmware currently
on the target, the target architecture, etc. The following table lists the supported
constraints:

Constraint                    | Description
------------------------------|------------
require-unmounted-destination | If `true`, require that the destination be completely unmounted by the OS before upgrading
require-partition1-offset     | Require that the block offset of partition 1 be the specified value

*Many more constraints to be added as needed*

Each task can have options to change how it is applied:

Option                        | Description
------------------------------|------------
verify-on-the-fly             | If `true`, the files are verified as they are written to the media. This speeds up processing and reduces memory in many cases.

The remainder of the `task` section is a list of event handlers. Event handlers are
organized as scopes. An event handler matches during the application of a firmware update
when an event occurs. Events include initialization, completion, errors, and files being
decompressed from the archive. Since archives are processed in a streaming manner, the
order of events is deterministice based on the order that files were added to the archive.
If it is important that one event happen before another, make sure that `file-resource`
sections are specified in the desired order. The following table lists supported events:

Event                         | Description
------------------------------|------------
on-init                       | First event sent when the task is applied
on-finish                     | Final event sent assuming no errors are detected during event processing
on-error                      | Sent if an error occurs so that intermediate files can be cleaned up
on-resource <resource name>   | Sent as events occur. Currently, this is sent as `file-resources` are processed from the archive. If `verify-on-the-fly` is set, then this event is sent at the start of the file. Otherwise, it is sent when the file has been read and verified completely.

The event scopes contain a list of actions. Actions can format file systems, copy files to file systems or
write to raw locations on the destination.

Action                                | Description
--------------------------------------|------------
raw_write(block_offset)               | Write the resource to the specified block offset
mbr_write(mbr)                        | Write the specified mbr to the target
fat_mkfs(block_offset, block_count)   | Create a FAT file system at the specified block offset and count
fat_write(block_offset, filename)     | Write the resource to the FAT file system at the specified block offset
fat_mv(block_offset, oldname, newname) | Rename the specified file on a FAT file system
fat_rm(block_offset, filename)        | Delete the specified file
fw_create(fwpath)                     | Create a firmware update archive in the specified place on the target (e.g., /tmp/on-reboot.fw)
fw_add_local_file(fwpath, name, local_path) | Add the specified local file to a firmware archive as the resource "name"


# License

This utility contains source code with various licenses. The bulk of the code is licensed with the Apache 2.0 license
which can be found in the `LICENSE` file.

The SHA-2 code comes from Aaron D. Gifford (http://www.aarongifford.com/) and has the following license:

```
Copyright (c) 2000-2001, Aaron D. Gifford
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTOR(S) ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTOR(S) BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
```

The FAT filesystem code (FatFs) comes from http://elm-chan.org/fsw/ff/00index_e.html
and has the following license.

```
FatFs module is a generic FAT file system module for small embedded systems.
This is a free software that opened for education, research and commercial
developments under license policy of following terms.

 Copyright (C) 2014, ChaN, all right reserved.

* The FatFs module is a free software and there is NO WARRANTY.
* No restriction on use. You can use, modify and redistribute it for
  personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
* Redistributions of source code must retain the above copyright notice.
```
