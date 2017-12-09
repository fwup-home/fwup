![The fwup pup](docs/fwup-pup.png)

# Overview

[![CircleCI](https://circleci.com/gh/fwup-home/fwup.svg?style=svg)](https://circleci.com/gh/fwup-home/fwup)
[![Coverage Status](https://coveralls.io/repos/github/fwup-home/fwup/badge.svg)](https://coveralls.io/github/fhunleth/fwup)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/4094/badge.svg)](https://scan.coverity.com/projects/4094)

`fwup` is a configurable image-based software update utility for embedded
Linux-based systems. It primarily supports software upgrade strategies that
update entire root filesystem images at once. This includes strategies like
swapping back and forth between A and B partitions, recovery partitions, and
various trial update/failback scenarios. All software update information is
combined into a ZIP archive that may optionally be cryptographically signed.
`fwup` has minimal dependencies and runtime requirements. Scripts are
intentionally limited to make failure scenarios easier to reason about.
Distribution of software update archives is not a feature. Users can call out to
`fwup` to run upgrades from external media, stream them from the network, or
script them using a tool like Ansible if so desired.

Here's a list of features:

1. Uses standard ZIP archives to make debugging and transmission simple.

1. Simple, but flexible configuration language to enable firmware updates on
   various platforms and firmware update policies.

1. Streaming firmware update processing to simplify target storage requirements.

1. Multiple firmware update task options per archive so that one archive can
   upgrade varying target configurations

1. Basic disk partitioning (GPT and MBR) and FAT filesystem manipulation

1. Human and machine readable progress.

1. Initialize or update SDCards on your development system whether you're
   running Linux, OSX, BSD, or Windows. MMC and SDCards are automatically
   detected and unmounted. No need to scan logs or manually unmount.

1. Firmware archive digital signature creation and verification

1. Delta update support using [VCDIFF](https://en.wikipedia.org/wiki/VCDIFF).
   See delta update section for details (BETA FEATURE).

1. Sparse file support to reduce number of bytes that need to be written when
   initializing large file systems (see section on sparse files)

1. Permissive license (Apache 2.0 License - see end of doc for details)

1. Extensively regression tested! Tests also provide examples of
    functionality.

Internally, `fwup` has many optimizations to speed up low level disk writes over
what can easily be achieved with `dd(1)`. It orders, Flash erase block aligns,
and can skip writing large unused sections to minimize write time. It also
verifies writes to catch corruption before the device reboots to the new
firmware. The goal is to make updates fast enough for code development and
reliable enough for production use.

# Installing

The simplest way to install `fwup` is via a package manager or installer.

On OSX, `fwup` is in [homebrew](http://brew.sh/):

```sh
brew install fwup
```

On Linux, download and install the appropriate package for your platform:

* [Debian/Ubuntu AMD64 .deb](https://github.com/fwup-home/fwup/releases/download/v1.8.3/fwup_1.8.3_amd64.deb)
* [Raspbian armhf .deb](https://github.com/fwup-home/fwup/releases/download/v1.8.3/fwup_1.8.3_armhf.deb)
* Alpine Linux - Install official [apk](https://pkgs.alpinelinux.org/packages?name=fwup&branch=edge)
* [RedHat/CentOS x86\_64 .rpm](https://github.com/fwup-home/fwup/releases/download/v1.8.3/fwup-1.8.3-1.x86_64.rpm)
* Arch Linux - See [fwup package](https://aur.archlinux.org/packages/fwup-git/) on AUR
* Buildroot - Support is included upstream since the 2016.05 release
* Yocto - See [meta-fwup](https://github.com/fwup-home/meta-fwup)

On Windows, `fwup` can be installed from [chocolatey](http://chocolatey.org)

    choco install fwup

Alternatively, download the [fwup executable](https://github.com/fwup-home/fwup/releases/download/v1.8.3/fwup.exe)
and place it in your path.

If you're using another platform or prefer to build it yourself, download the
latest [source code release](https://github.com/fwup-home/fwup/releases/download/v1.8.3/fwup-1.8.3.tar.gz)
or clone this repository. Then read one of the following files:

* [Linux build instructions](docs/build_linux.md)
* [OSX build instructions](docs/build_osx.md)
* [Windows build instructions](docs/build_windows.md)
* [Raspbian build instructions](docs/build_rpi.md)
* [FreeBSD/NetBSD/OpenBSD build instructions](docs/build_bsd.md)

When building from source, please verify that the regression test pass on your
system (run `make check`) before using `fwup` in production. While the tests
usually pass, they have found minor issues in third party libraries in the past
that really should be fixed.

NOTE: For space-constrained target devices, use `./configure
--enable-minimal-build` to trim functionality that's rarely used.

# Invoking

If you were given a `.fw` file and just want to install its contents on an
SDCard, here's an example run on Linux:

```sh
$ sudo fwup example.fw
Use 14.92 GiB memory card found at /dev/sdc? [y/N] y
100%
Elapsed time: 2.736s
```

If you're on OSX or Windows, leave off the call to `sudo`.

IMPORTANT: If you're using an older version of `fwup`, you'll have to specify
more arguments. Run: `fwup -a -i example.fw -t complete`

Here's a list of other options:

```plaintext
Usage: fwup [OPTION]...

Options:
  -a, --apply   Apply the firmware update
  -c, --create  Create the firmware update
  -d <file> Device file for the memory card
  -D, --detect List attached SDCards or MMC devices and their sizes
  -E, --eject Eject removable media after successfully writing firmware.
  --no-eject Do not eject media after writing firmware
  --enable-trim Enable use of the hardware TRIM command
  --exit-handshake Send a Ctrl+Z on exit and wait for stdin to close (Erlang)
  -f <fwup.conf> Specify the firmware update configuration file
  -F, --framing Apply framing on stdin/stdout
  -g, --gen-keys Generate firmware signing keys (fwup-key.pub and fwup-key.priv)
  -i <input.fw> Specify the input firmware update file (Use - for stdin)
  -l, --list   List the available tasks in a firmware update
  -m, --metadata   Print metadata in the firmware update
  -n   Report numeric progress
  -o <output.fw> Specify the output file when creating an update (Use - for stdout)
  -p, --public-key-file <keyfile> A public key file for verifying firmware updates
  --private-key <key> A private key for signing firmware updates
  --progress-low <number> When displaying progress, this is the lowest number (normally 0 for 0%)
  --progress-high <number> When displaying progress, this is the highest number (normally 100 for 100%)
  --public-key <key> A public key for verifying firmware updates
  -q, --quiet   Quiet
  -s, --private-key-file <keyfile> A private key file for signing firmware updates
  -S, --sign Sign an existing firmware file (specify -i and -o)
  --sparse-check <path> Check if the OS and file system supports sparse files at path
  --sparse-check-size <bytes> Hole size to check for --sparse-check
  -t, --task <task> Task to apply within the firmware update
  -u, --unmount Unmount all partitions on device first
  -U, --no-unmount Do not try to unmount partitions on device
  --unsafe Allow unsafe commands (consider applying only signed archives)
  -v, --verbose   Verbose
  -V, --verify  Verify an existing firmware file (specify -i)
  --verify-writes Verify writes when applying firmware updates to detect corruption (default for writing to device files)
  --no-verify-writes Do not verify writes when applying firmware updates (default for regular files)
  --version Print out the version
  -y   Accept automatically found memory card when applying a firmware update
  -z   Print the memory card that would be automatically detected and exit
  -1   Fast compression (for create)
  -9   Best compression (default)

Examples:

Create a firmware update archive:

  $ fwup -c -f fwup.conf -o myfirmware.fw

Apply the firmware to an attached SDCard. This would normally be run on the host
where it would auto-detect an SDCard and initialize it using the 'complete' task:

  $ fwup -a -i myfirmware.fw -t complete

Apply the firmware update to /dev/sdc and specify the 'upgrade' task:

  $ fwup -a -d /dev/sdc -i myfirmware.fw -t upgrade

Create an image file from a .fw file for use with dd(1):

  $ fwup -a -d myimage.img -i myfirmware.fw -t complete

Generate a public/private key pair:

  $ fwup -g

Store fwup-key.priv in a safe place and fwup-key.pub on the target. To sign
an existing archive run:

  $ fwup -S -s fwup-key.priv -i myfirmware.fw -o signedfirmware.fw
```

# Example usage

The regression tests contain short examples for usage of various script
elements and are likely the most helpful to read due to their small size.

Other examples can be found in the
[bbb-buildroot-fwup](https://github.com/fhunleth/bbb-buildroot-fwup) for project
for the BeagleBone Black and Raspberry Pi. The [Nerves
Project](https://github.com/nerves-project/nerves_system_br) has more examples
and is better maintained.

My real world use of `fwup` involves writing the new firmware to a place on the
Flash that's not in current use and then 'flipping' over to it at the very end.
The examples tend to reflect that. `fwup` can also be used to overwrite an
installation in place assuming you're using an initramfs, but that doesn't give
protection against someone pulling power at a bad time. Also, `fwup`'s one pass
over the archive feature means that firmware validation is mostly done on the
fly, so you'll want to verify the archive first (see the `-V` option).

# Helper scripts

While not the original use of `fwup`, it can be convenient to convert other
files to  `.fw` files. `fwup` comes with the following shell script helper:

* `img2fwup` - convert a raw image file to a `.fw` file

A use case for the `img2fwup` script is to convert a large SDCard image file
to one that is compressed and checksummed by `fwup` for distribution.

# Versioning

`fwup` uses [semver](https://semver.org) for versioning. For example, if you are
using 1.0.0, that means no breaking changes until 2.0.0 or new features until
1.1.0. I highly recommend a conservative approach to upgrading `fwup` once you
have devices in the field. For example, if you have 1.0.0 devices in the field,
it's ok to update to 1.1.0, but be very careful about using 1.1.0 features in
your `fwup.conf` files. They will be ignored by 1.0.0 devices and that may or
may not be what you want.

# Configuration file format

`fwup` uses the Unix configuration library,
[libconfuse](https://github.com/libconfuse/libconfuse), so its configuration has some
similarities to other programs. The configuration file is used to create
firmware archives. During creation, `fwup` embeds a processed version of the
configuration file into the archive that has been stripped of comments, has had
all variables resolved, and has some additional useful metadata added.
Configuration files are organized into scoped blocks and options are set using
a `key = value` syntax. Additionally, configuration files may include
configuration fragments and other files by calling `include("filename")`.

## Environment variables

For integration into build systems and other scripts, `fwup` performs
variable substitution inside of the configuration files. Keep in
mind that environment variables are resolved on the host during firmware update
file creation. Generated firmware files do not contain

Environment variables are referenced as follows:

    key = ${ANY_ENVIRONMENT_VARIABLE}

It is possible to provide default values for environment variables using the
`:-` symbol:

    key = ${ANY_ENVIRONMENT_VARIABLE:-adefault}

Inside configuration files, it can be useful to define constants that are used
throughout the file. Constants are referenced identically to environment
variables. Here is an example:

    define(MY_CONSTANT, 5)

By default, repeated definitions of the same constant do not change that
constant's value. In other words, the first definition wins. Note that the first
definition could come from a similarly named environment variable. This makes
it possible to override a constant in a build script.

In some cases, having the last definition win is preferable for constants that
never ever should be overridden by the environment or by earlier calls to
`define`. For this behavior, use `define!`:

    define!(MY_CONSTANT, "Can't override this")

Simple math calculations may also be performed using `define-eval()` and
`define-eval!()`. For example:

    define-eval(AN_OFFSET, "${PREVIOUS_OFFSET} + ${PREVIOUS_COUNT}")

These two functions were added in release 0.10.0, but since
they are evaluated at firmware creation time, .fw files created using them are
compatible with older versions of `fwup`.

Finally, `file-resource` will define a variable named `FWUP_SIZE_<resource_name>` with the size of the resource.  For example
the following will create a variable named `FWUP_SIZE_zImage`:

```conf
file-resource zImage {
        host-path = "output/images/zImage"
}

execute("echo zImage size is ${FWUP_SIZE_zImage} bytes")
```

## Global scope

At the global scope, the following options are available:

Key                  | Description
---------------------|------------
require-fwup-version | Require a minimum version of fwup to apply this update
meta-product         | Product name
meta-description     | Description of product or firmware update
meta-version         | Firmware version
meta-author          | Author or company behind the update
meta-platform        | Platform that this update runs on (e.g., rpi or bbb)
meta-architecture    | Platform architectures (e.g., arm)
meta-vcs-identifier  | A version control identifier for use in reproducing this image
meta-misc            | Miscellaneous additional data. Format and contents are up to the user
meta-creation-date   | Timestamp when the update was created (derived from ZIP metadata). For reproducible builds, set the [`SOURCE_DATE_EPOCH`](https://reproducible-builds.org/specs/source-date-epoch/#idm55) environment variable.
meta-fwup-version    | Version of fwup used to create the update (deprecated - no longer added since fwup 1.2.0)
meta-uuid            | A UUID to represent this firmware. The UUID won't change even if the .fw file is digitally signed after creation (automatically generated)

After setting the above options, it is necessary to create scopes for other options. The
currently available scopes are:

Scope                | Description
---------------------|------------
file-resource        | Defines a reference to a file that should be included in the archive
mbr                  | Defines the master boot record contents on the destination
gpt                  | Defines GPT partitions on the destination
task                 | Defines a firmware update task (referenced using -t from the command line)

## file-resource

A `file-resource` specifies a file on the host that should be included in the
archive. Each `file-resource` should be given a unique name so that it can be
referred to by other parts of the update configuration. `fwup` will
automatically record the length and BLAKE2b-256 hash of the file in the
archive. These fields are used internally to compute progress and verify the
contents of the archive. A typical `file-resource` section looks like this:

```conf
file-resource zImage {
        host-path = "output/images/zImage"
}
```

Resources are usually stored in the `data` directory of the firmware archive.
This is transparent for most users. If you need to make the `.fw` file
work with other software, it is sometimes useful to embed a file into
the archive at another location. This can be done by specifying an absolute
path resource as follows:

```conf
file-resource "/my_custom_metadata" {
        host-path = "path/to/my_custom_metadata_file"
}
```

### Resource concatenation

Sometimes you need to concatenate multiple files together to form one
`file-resource`. While you can sometimes do this using multiple calls to
`raw_write`, that won't work if you don't know the file offsets a priori or the
offsets don't fall on block boundaries. Another alternative is to concatenate
files as a prep step to fwup. If that's inconvenient, `fwup` allows multiple
paths to be specified in `host-path` that are separated by semicolons. They
will be concatenated in the order they appear.

```conf
file-resource kernel_and_rootfs {
        # Concatenate uImage and the rootfs. OpenWRT mtd splitter will
        # separate them back out at runtime.
        host-path = "output/images/uImage;output/images/rootfs.squashfs"
}
```

### File resource validation checks

When creating archives, `fwup` can perform validation checking on file
resources to catch simple errors. These checks can catch common errors like
file resources growing too large to fit on the destination or files truncated
due to cancelled builds.

Note that these checks are not performed when applying updates, since the
actual length (and a hash) is recorded in the archive metadata and used for
verification.

The following checks are supported:

Check           | Description
----------------|------------
assert-size-lte | If the file size is not less than or equal the specified amount, report an error.
assert-size-gte | If the file size is not greater than or equal the specified amount, report an error.

Sizes are given in 512 byte blocks (like everything else in `fwup`).

Here's an example:

```conf
file-resource rootfs.img {
        host-path = "output/images/rootfs.squashfs"
        assert-size-lte = ${ROOTFS_A_PART_COUNT}
}
```

### Files from strings

Sometimes it's useful to create short files inside the `fwup` config file
rather than referencing them. This can be accomplished using the `contents` key
in a `file-resource`. Variable substitution works in the `contents` string just
like any other string in the `fwup` configuration file.

```conf
file-resource short-file.txt {
        contents = "You're looking at a short\n\
file creating by fwup.\n\
When it was made, FOO was ${FOO}.\n"
}
```

## mbr

A `mbr` section specifies the contents of the Master Boot Record on the
destination media. This section contains the partition table that's read by
Linux and the bootloaders for finding the file systems that exist on the media.
In comparison to a tool like `fdisk`, `fwup` only supports simplistic partition
setup, but this is sufficient for many devices. Tools such as `fdisk` can be
used to determine the block offsets and sizes of partitions for the
configuration file. Offsets and sizes are given in 512 byte blocks. Here's a
potential mbr definition:

```conf
mbr mbr-a {
        bootstrap-code-host-path = "path/to/bootstrap-data" # should be 440 bytes
        signature = 0x01020304

        partition 0 {
                block-offset = ${BOOT_PART_OFFSET}
                block-count = ${BOOT_PART_COUNT}
                type = 0x1 # FAT12
                boot = true
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

If you're using an Intel Edison or similar platform, `fwup` supports generation
of the OSIP header in the MBR. This header provides information for where to
load the bootloader (e.g.., U-Boot) in memory. The `include-osip` option
controls whether the header is generated. The OSIP and image record (OSII)
option names map directly to the header fields with the exception that length,
checksum and image count fields are automatically calculated. The following is
an example that shows all of the options:

```conf
mbr mbr-a {
    include-osip = true
    osip-major = 1
    osip-minor = 0
    osip-num-pointers = 1

    osii 0 {
        os-major = 0
        os-minor = 0
        start-block-offset = ${UBOOT_OFFSET}
        ddr-load-address = 0x01100000
        entry-point = 0x01101000
        image-size-blocks = 0x0000c000
        attribute = 0x0f
    }

    partition 0 {
        block-offset = ${ROOTFS_A_PART_OFFSET}
        block-count = ${ROOTFS_A_PART_COUNT}
        type = 0x83 # Linux
    }
}
```

Sometimes it's useful to have the final partition fill the remainder of the
storage. This is needed if your target's storage size is unknown and you need
to use as much of it as possible. The `expand` option requests that `fwup`
grow the `block-count` to be as large as possible. When using `expand`, the
`block-count` is now the minimum partition size. Only the final partition can
be expandable. Here's an example:

```conf
mbr mbr-a {
        partition 0 {
                block-offset = ${BOOT_PART_OFFSET}
                block-count = ${BOOT_PART_COUNT}
                type = 0x1 # FAT12
                boot = true
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
                expand = true
        }
}
```

## gpt

A `gpt` section specifies the contents of a [GUID Partition
Table](https://en.wikipedia.org/wiki/GUID_Partition_Table). It is similar to the
`mbr` section, but it supports more partitions and uses UUIDs. Here's an example
gpt definition:

```conf
gpt my-gpt {
    # UUID for the entire disk
    guid = b443fbeb-2c93-481b-88b3-0ecb0aeba911

    partition 0 {
        block-offset = ${EFI_PART_OFFSET}
        block-count = ${EFI_PART_COUNT}
        type = c12a7328-f81f-11d2-ba4b-00a0c93ec93b # EFI type UUID
        guid = 5278721d-0089-4768-85df-b8f1b97e6684 # ID for partition 0 (create with uuidgen)
        name = "efi-part.vfat"
    }
    partition 1 {
        block-offset = ${ROOTFS_PART_OFFSET}
        block-count = ${ROOTFS_PART_COUNT}
        type = 44479540-f297-41b2-9af7-d131d5f0458a # Rootfs type UUID
        guid = fcc205c8-2f1c-4dcd-bef4-7b209aa15cca # Another uuidgen'd UUID
        name = "rootfs.ext2"
        flags = 0xc000000000000
        boot = true
    }
}
```

See the GPT partition entry header specification for what `partition` fields
mean and how to use them. Of the fields, `name`, `flags`, and `boot` are
optional and default to an empty string, zero, or false. The `flags` field holds
the integer value of the partition attribute field. It is a 64-bit number. Bit
2, the legacy BIOS bootable flag, can also be set by specifying `boot = true`.
Both `boot` and `flags` can be specified in a `partition` block.

Call `gpt_write` to tell `fwup` to write the protective MBR and primary and
backup GPT tables.

## U-Boot environment

For systems using the U-Boot bootloader, some support is included for modifying
U-Boot environment blocks. In order to take advantage of this, you must declare
a `uboot-environment` section at the top level that describes how the
environment block:

```conf
uboot-environment my_uboot_env {
    block-offset = 2048
    block-count = 16
}
```

To use the redundant environment block style, add block-offset-redund with the
address where the redundant copy is located:

```conf
uboot-environment my_uboot_env {
    block-offset = 2048
    block-count = 16
    block-offset-redund = 2064
}
```

See the functions in the task section for getting and setting U-Boot variables.

NOTE: Currently, I've only implemented support for U-Boot environments that I
use. Notably, this doesn't support big endian targets, and writes to raw NAND
parts. Please consider contributing back support for these if you use them.

If `fwup`'s U-Boot support does not meet your needs, it is always possible to
create environment images using the `mkenvimage` utility and `raw_write` them to
the proper locations. This is probably more appropriate when setting lots of
variables.

## task

The `task` section specifies a firmware update task. These sections are the
main part of the firmware update archive since they describe the conditions
upon which an update is applied and the steps to apply the update. Each `task`
section must have a unique name, but when searching for a task, the firmware
update tool only does a prefix match. This lets you define multiple tasks that
can be evaluated based on conditions on the target hardware. The first matching
task is the one that gets applied. This can be useful if the upgrade process is
different based on the version of firmware currently on the target, the target
architecture, etc. The following table lists the supported constraints:

Constraint                                         | Min fwup version | Description
---------------------------------------------------|------------------|------------
require-fat-file-exists(block_offset, filename)    | 0.7.0 | Require that a file exists in the specified FAT filesystem
require-fat-file-match(block_offset, filename, pattern) | 0.14.0 | Require that filename exists and that pattern matches bytes inside of the file
require-partition-offset(partition, block_offset)  | 0.7.0 | Require that the block offset of a partition be the specified value
require-path-on-device(path, device)               | 0.13.0 | Require that the specified path (e.g., "/") is on the specified partition device (e.g., "/dev/mmcblk0p1")
require-path-at-offset(path, offset)               | 0.19.0 | Require that the specified path (e.g., "/") is at the specified block offset (e.g., 1024). Combine with require-path-on-device.
require-uboot-variable(my_uboot_env, varname, value) | 0.10.0 | Require that a variable is set to the specified value in the U-Boot environment

The remainder of the `task` section is a list of event handlers. Event handlers
are organized as scopes. An event handler matches during the application of a
firmware update when an event occurs. Events include initialization,
completion, errors, and files being decompressed from the archive. Since
archives are processed in a streaming manner, the order of events is
deterministic based on the order that files were added to the archive. If it is
important that one event happen before another, make sure that `file-resource`
sections are specified in the desired order. The following table lists
supported events:

Event                         | Description
------------------------------|------------
on-init                       | First event sent when the task is applied
on-finish                     | Final event sent assuming no errors are detected during event processing
on-error                      | Sent if an error occurs so that intermediate files can be cleaned up
on-resource &lt;resource name>   | Sent as events occur. Currently, this is sent as `file-resources` are processed from the archive.

The event scopes contain a list of actions. Actions can format file systems, copy files to file systems or
write to raw locations on the destination.

Action                                  | Min fwup version | Description
----------------------------------------|------------------|------------
error(message)                          | 0.12.0 | Immediately fail a firmware update with an error
execute(command)                        | 0.16.0 | Execute a command on the host. Requires the `--unsafe` flag
fat_mkfs(block_offset, block_count)     | 0.1.0 | Create a FAT file system at the specified block offset and count
fat_write(block_offset, filename)       | 0.1.0 | Write the resource to the FAT file system at the specified block offset
fat_attrib(block_offset, filename, attrib) | 0.1.0 | Modify a file's attributes. attrib is a string like "RHS" where R=readonly, H=hidden, S=system
fat_mv(block_offset, oldname, newname)  | 0.1.0 | Rename the specified file on a FAT file system
fat_mv!(block_offset, oldname, newname) | 0.14.0 | Rename the specified file even if newname already exists.
fat_rm(block_offset, filename)          | 0.1.0 | Delete the specified file
fat_mkdir(block_offset, filename)       | 0.2.0 | Create a directory on a FAT file system. This also succeeds if the directory already exists.
fat_setlabel(block_offset, label)       | 0.2.0 | Set the volume label on a FAT file system
fat_touch(block_offset, filename)       | 0.7.0 | Create an empty file if the file doesn't exist (no timestamp update like on Linux)
gpt_write(gpt)                          | 1.4.0 | Write the specified GPT to the target
info(message)                           | 0.13.0 | Print out an informational message
mbr_write(mbr)                          | 0.1.0 | Write the specified mbr to the target
path_write(destination_path)            | 0.16.0 | Write a resource to a path on the host. Requires the `--unsafe` flag
pipe_write(command)                     | 0.16.0 | Pipe a resource through a command on the host. Requires the `--unsafe` flag
raw_memset(block_offset, block_count, value) | 0.10.0 | Write the specified byte value repeatedly for the specified blocks
raw_write(block_offset, options)        | 0.1.0 | Write the resource to the specified block offset. Options include `cipher` and `secret`.
trim(block_offset, count)               | 0.15.0 | Discard any data previously written to the range. TRIM requests are issued to the device if --enable-trim is passed to fwup.
uboot_recover(my_uboot_env)             | 0.15.0 | If the U-Boot environment is corrupt, reinitialize it. If not, then do nothing
uboot_clearenv(my_uboot_env)            | 0.10.0 | Initialize a clean, variable free U-boot environment
uboot_setenv(my_uboot_env, name, value) | 0.10.0 | Set the specified U-boot variable
uboot_unsetenv(my_uboot_env, name)      | 0.10.0 | Unset the specified U-boot variable

## Delta firmware updates (BETA)

The purpose of delta firmware updates is to reduce firmware update file sizes
and their associated costs by sending patches to devices. This requires that one
know what firmware is running on the device so that an appropriate patch can be
made. As with other features, `fwup` only addresses the firmware update file
processing piece, but assists in this process by 1. uniquely identifying
firmware via UUIDs and 2. including
[`xdelta3`](https://github.com/jmacd/xdelta/tree/release3_1_apl/xdelta3)
decompression support.

This feature is currently BETA, since it may change in backwards incompatible
ways based on trial deployments. If you're using this, please avoid deploying it
to places that are hard to access just in case.

`fwup` cannot produce delta `.fw` archives automatically. However, they are
easy to produce manually or via a script. The deployments in progress use
scripts to create patches for upgrading all possible firmware versions (keyed
off UUID). Here's how:

1. Decide which file resource is a good candidate for delta updates (you can
   pick more than one). Call this `rootfs.img`.
2. In the `fwup.conf`, in the `on-resource rootfs.img` handlers, specify where
   the currently running rootfs.img contents exist. For example, if you're
   doing an A/B upgrades, when you upgrade B, point to the A's rootfs for
   source contents and vice versa for upgrading A. It will look something
   like this:

   ```txt
   task upgrade.a {
       on-resource rootfs.img {
           delta-source-raw-offset=${ROOTFS_B_PART_OFFSET}
           delta-source-raw-count=${ROOTFS_B_PART_COUNT}
           raw_write(${ROOTFS_A_PART_OFFSET})
       }
   }
   task upgrade.b {
       on-resource rootfs.img {
           delta-source-raw-offset=${ROOTFS_A_PART_OFFSET}
           delta-source-raw-count=${ROOTFS_A_PART_COUNT}
           raw_write(${ROOTFS_B_PART_OFFSET})
       }
   }
   ```

3. Obtain the firmware file for the software running on the device. Call this
   `original.fw`.
4. Create the firmware file for the new software. Call this `update.fw`. If
   you normally sign your firmware files, you can sign it now or sign the
   resulting patch file depending on what works best for your release process.
5. Run `unzip` on both `original.fw` and `update.fw` in different directories.
   The `rootfs.img` file can be found under the `data` directory when unzipped.
6. `mkdir -p my_patch/data`
7. `xdelta3 -A -S -f -s original/data/rootfs.img update/data/rootfs.img my_patch/data/rootfs.img`
8. `cp update.fw my_patch/delta_update.fw`
9. `cd my_patch && zip delta_update.fw data/rootfs.img`

The `delta_update.fw` will have the patch for the `rootfs.img` and should be
quite a bit smaller. If not, check that your before and after `rootfs.img` files
don't have a lot of timestamp changes or are already so compressed that the
deltas propagate through the entire image.

Even though you're manually modifying the `.fw` file, `fwup` stills provide
guarantees on the final bits installed on the device. For one, `fwup` computes
Blake 2B hashes on the final contents of the files, so what matters is what
comes out of the `xdelta3` decompressor and not what is stored in the archive.
Most likely though, `xdelta3` will detect corruption since it checks Adler32
checksums as it decompresses.

### Delta update on-resource source settings

Where to find the source ("before" version) is always specified in `on-resource`
blocks. It can either be source from raw bytes or from files in a FAT-formatted
partition. Only one source is supported in each `on-resource` block.

Key                               | Description
----------------------------------|------------
delta-source-raw-offset           | Set to the starting block offset
delta-source-raw-count            | Set to the number of blocks in the source region. No reads will be allowed outside of this area.
delta-source-fat-offset           | Not implemented yet
delta-source-fat-path             | Not implemented yet

## Sparse files

Sparse files are files with gaps in them that are only represented on the
filesystem in metadata. Not all filesystems support sparse files, but in
general, Linux has good support. Creating a sparse file is easy: just seek to a
location past the end of file and write some data. The gap is "stored" as a
hole in the filesystem metadata. Data is read back from the hole as zeros. Data
and holes are restricted to start and end on filesystem block boundaries, so
small gaps may be filled in with zeros rather than being stored as a hole.

Why is this important? If you're using `fwup` to write a large EXT2 partition,
you'll find that it contains many gaps. It would be better to just write the
EXT2 data and metadata without filling in all of the unused space. Sparse file
support in `fwup` lets you do that. Since EXT2 filesystems legitimately contain
long runs of zeros that must be written to Flash, `fwup` queries the filesystem
containing the EXT2 data to find the gaps. Other tools like `dd(1)` only look
for runs of zeros so their sparse file support cannot be used to emulate this.
You may see warnings about copying sparse files to Flash and it has to do with
tools not writing long runs of zeros. The consequence of `fwup` querying the
filesystem for holes is that this feature only works when firmware update
archives are created on operating systems and filesystems that support it. Of
course, firmware updates can be applied on systems without support for querying
holes in files. Those systems also benefit from not having to write as much to
Flash devices. If you instead apply a firmware update to a normal file, though,
the OS will likely fill in the gaps with zeros and thus offer no improvement.

There is one VERY important caveat with the sparse file handling: Some zeros in
files are important and some are not. If runs in zeros in a file are important
and they are written to a file as a "hole", `fwup` will not write them back.
This is catastrophic if the zeros represent things like free blocks on a
filesystem. Luckily, the file system formatting utilities write the important
zeros to the disk and the OS does not scan bytes to see which ones are runs on
zeros and automatically create holes. Programs like `dd(1)` can do this, though,
so it is crucial that you do not run files through `dd` to make then sparser
before passing them to `fwup`.

This feature is off by default. To turn this feature on, set `skip-holes` on
the resource to `true`:

```conf
file-resource rootfs.img {
        host-path = "output/images/rootfs.img"
        skip-holes = true
}
```

## Disk encryption

The `raw_write` function has limited support for disk encryption that's
compatible with the Linux `dm-crypt` kernel driver. This makes it possible to
write filesystem data in a way that's unreadable at rest. Caveats are in order:

1. `fwup` does not address handling of secret keys and improper handling can
   easily compromise the benefits of encryption
2. The `.fw` archive is not encrypted. This mechanism assumes that the secrecy
   of the archive is protected by other means. Of course, it is possible
   pre-encrypt the data in the archive, but then you can't have device-specific
   secret keys.
3. Only the simplest `dm-crypt` cipher is currently supported ("aes-cbc-plain").
   This has known deficiencies. PRs for other modes that can be incorporated
   under `fwup`'s Apache License would be appreciated

Various tutorials exist on the Internet for creating encrypted filesystems and
mounting filesystems using `dm-crypt`. `fwup` is much simpler. It takes a block
of bytes to write, encrypts it, and writes it to the destination. For example,
if you have a SquashFS-formatted filesystem that you want written encrypted to a
disk, you'd have this fragment:

```conf
on-resource fs.squashfs {
    raw_write(${PARTITION_START}, "cipher=aes-cbc-plain", "secret=\${SECRET_KEY}")
}
```

In the above example, the `SECRET_KEY` is expected to come from an environment
variable being set on the device when applying the firmware update. You could,
of course, hard-code the secret key in the configuration file to test things
out. The key is hex-encoded.

Then, on the device, mount the SquashFS partition but use `dm-crypt`. The
process will look something like this:

```sh
losetup /dev/loop0 /dev/mmcblk0p2
cryptsetup open --type=plain --cipher=aes-cbc-plain --key-file=key.txt /dev/loop0 my-filesystem
mount /dev/mapper/my-filesystem /mnt
```

You will likely need to replace many of the arguments above with ones
appropriate for your system.

## Reproducible builds

It's possible for the system time to be saved in various places when using
`fwup`. This means that an archive with the same contents, but built at
different times results in `.fw` files with different bytes. See
[reproducible-builds.org](https://reproducible-builds.org/) for a discussion on
this topic.

`fwup` obeys the
[`SOURCE_DATE_EPOCH`](https://reproducible-builds.org/docs/source-date-epoch/)
 environment variable and will force all timestamps to the value of that
variable when needed. Set `$SOURCE_DATE_EPOCH` to the number of seconds since
midnight Jan 1, 1970 (run `date +%s`) to use this feature.

A better way of comparing `.fw` archives, though, is to use the firmware UUID.
The firmware UUID is computed from the contents of the archive rather than the
bit-for-bit representation of the `.fw` file, itself. The firmware UUID is
unaffected by timestamps (with or without `SOURCE_DATE_EPOCH`) or other things
like compression algorithm improvements. This is not to say that
`SOURCE_DATE_EPOCH` is not important, but that the UUID is an additional tool
for ensuring that firmware updates are reproducible.

# Firmware authentication

Firmware archives can be authenticated using a simple public/private key
scheme. To get started, create a public/private key pair by invoking `fwup -g`.
The algorithm used is [Ed25519](http://ed25519.cr.yp.to/). This generates two
file: `fwup-key.pub` and `fwup-key.priv`. It is critical to keep the signing
key, `fwup-key.priv` secret.

To sign an archive, pass `-s fwup-key.priv` to fwup when creating the firmware.
The other option is to sign the firmware archive after creation with `--sign`
or `-S`.

To verify that an archive has been signed, pass `-p fwup-key.pub` on the
command line to any of the commands that read the archive. E.g., `-a`, `-l` or
`-m`.

It is important to understand how verification works so that the security of
the archive isn't compromised. Firmware updates are applied in one pass to
avoid needing a lot of memory or disk space. The consequence of this is that
verification is done on the fly. The main metadata for the archive is always
verified before any operations occur. Cryptographic hashs (using the
[BLAKE2b-256](https://blake2.net/) algorithm) of each file contained in the
archive is stored in the metadata. The hash for each file is computed on the
fly, so a compromised file may not be detected until it has been written to
Flash. Since this is obviously bad, the strategy for creating firmware updates
is to write them to an unused location first and then switch over at the last
possible minute. This is desirable to do anyway, since this strategy also
provides some protection against the user disconnecting power midway through
the firmware update.

# Integration with applications

It is expected that many users will want to integrate `fwup` with their
applications. Many operations can be accomplished by just invoking the `fwup`
executable and parsing the text written to `stdout`. When applying firmware
progress updates are delivered based on commandline options:

  1. Human readable - This is the default. Progress is updated from the text `0%` to `100%`.
  1. Numeric (`-n`) - Progess is printed as `0\n` to `100\n`
  1. Quiet (`-q`) - No progress is printed

While the above works well for scripts and when errors can be seen by the
operator, `fwup` supports a structured use of `stdin`/`stdout` as well. Specify
the `--framing` option to any of the commands to use this option.

The framing feature is influenced by the Erlang VM's port API and should be
relatively easy to integrate with non-Erlang VM languages. The framing works
around deficiencies in the built-in interprocess communication. For example, by
enabling framing, a program can stream a firmware update through `fwup's`
`stdin` without needing to close its `stdout` to signal end of file. Another
feature aided by framing is knowing what text goes together and whether the
text is part of an error message or not. Exit status is still an indicator of
success or failure, but the controlling application doesn't need to wait for
the program to exit to know what happened.

In `--framing` mode, all communication with `fwup` is done in packets (rather
than byte streams). A packet starts with a 4 byte length field. The length is a
big endian (network byte order) unsigned integer. A zero-length packet (i.e., 4
bytes of zeros) signals end of input.

Field          | Size         | Description
---------------|--------------|-------------
Length         | 4 bytes      | Packet length as a big endian integer
Data           | Length bytes | Payload

Input and output packets have different formats. For sending input to `fwup`
(like when streaming a `.fw` file using stdio), the input bytes should be
framed into packets however is most convenient. For example, if bytes are
received in 4K chunks, then they can be sent to `fwup` in 4K packets with a
zero-length packet at the end. The packets need not be the same size.

All output packets from `fwup` have a 2 byte type field at the beginning of the
packet:

Field          | Size           | Description
---------------|----------------|-------------
Length         | 4 bytes        | Packet length as a big endian integer
Type           | 2 bytes        | See below
Data           | Length-2 bytes | Payload

The following types are defined:

Type           | 2 byte value | Description
---------------|--------------|------------
Success        | "OK"         | The command was executed successfully. The payload is a 2 bytes result code (currently 0 for success) followed by an optional textual message.
Error          | "ER"         | A failure occurred. The payload is a 2 byte error code (future use) followed by a textual error message.
Warning        | "WN"         | A warning occurred. The payload is a 2 byte error code (future use) followed by a textual error message.
Progress       | "PR"         | The next two bytes are the progress (0-100) as a big endian integer.

A related option is `--exit-handshake`. This option was specifically implemented
for Erlang to support integration with its port process feature. It may be
useful for other integrations where it's more convenient to wait for a final
character coming from a subprocess rather than watching for an exit. The
problem with Erlang is that it's easy for the message that the process exited to
beat the final characters coming out stdout. When this option is enabled, `fwup`
expects the calling process to close `stdin` when it's ready for `fwup` to exit.

# FAQ

## How do I include a file in the archive that isn't used by fwup

The scenario is that you need to store some metadata or some other information
that is useful to another program. For example, you'd like to include some
documentation or other notes inside the firmware update archive that the
destination device can pull out and present on a UI. To do this, just add
`file-resource` blocks for each file. These blocks don't need to be referenced
by an `on-resource` block.

## How do I include the firmware version in the archive

If you are using git, you can invoke `fwup` as follows:

```sh
GITDESCRIBE=`git describe` fwup -c -f myupdate.conf -o out.fw
```

Then in `myupdate.conf` add the line:

```conf
meta-version = "${GITDESCRIBE}"
```

On the target device, you can retrieve the version by using `-m`. For example:

```sh
$ fwup -m -i out.fw
meta-product = "Awesome product"
meta-description = "A description"
meta-version = "v0.0.1"
meta-author = "Me"
meta-platform = "bbb"
meta-architecture = "arm"
meta-uuid="07a34e75-b7ea-5ed8-b5d9-80c10daf4939"
```

## What's something cool that you can do with fwup

Ok, this isn't really a FAQ, but for some reason people think this is cool. Many
systems that I work on are network connected with ssh. Sometimes I update them
by doing this:

```sh
$ cat mysoftware.fw | ssh root@192.168.1.20 \
    'fwup -a -U -d /dev/mmcblk2 -t upgrade && reboot'
```

The ability to pipe software updates through `fwup` comes in handy. This has
also gotten me out of situations where, for whatever reason, I no longer had enough
space to store the update on the device.

## How should I implement multiple signing keys

There are several use cases where it's necessary to have multiple signing keys
in use at a time. For example, you could want to enforce that all firmware
updates are signed in your infrastructure, but not force everyone to go though
the official secure path for QA builds. You may also want to only use each key
for a limited amount of signings.

Currently, each firmware file can only have one signature. However, the
verifier (device) can specify multiple public keys (repeated -p options). While
it is possible to call `fwup` for each key, specifying multiples keys is
recommended to run the verification through fwup to support streamed updates
and also to simplify this critical code path.

## How do I debug

I apply updates to regular files on my laptop (as opposed to eMMC or SDCards)
and examine them with a hex editor. A few other routes might be useful too:

1. Unzip the .fw file to inspect the contents. It's a regular ZIP file and
   the `meta.conf` file inside it is the stripped down view of what your
   configuration looks like after variable substitution, etc.
1. Add the `error()` function to do printf-style debugging.
1. Find an image that works and skip updating some sections. For example,
   some processors are very picky on the MBR contents and it's easier to
   get everything else working before tackling partition constraints.

## How do I specify the root partition in Linux

There are a few options. Most people can specify `root=/dev/mmcblk0p1` or
`root=/dev/sda1` or something similar on the kernel commandline and everything
will work out fine. On systems with multiple drives and an unpredictable boot
order, you can specify `root=PARTUUID=01234567-01` where the `-01` part
corresponds to the 1-based partition index and `01234567` is any signature. In
your `fwup.conf` file's MBR block, specify `signature = 0x01234567`. A third
option is to use an initramfs and not worry about any of this.

## How do I get the best performance

In general, `fwup` writes to Flash memory in large blocks so that the update
can occur quickly. Obviously, reducing the amount that needs to get written
always helps. Beyond that, most optimizations are platform dependent. Linux
caches writes so aggressively that writes to Flash memory are nearly as fast as
possible. OSX, on the other hand, does very little caching, so doing things
like only working with one FAT filesystem at a time can help. In this case,
`fwup` only caches writes to one FAT filesystem at a time, so mixing them will
flush caches. OSX is also slow to unmount disks, so keep in mind that
performance can only be so fast on some systems.

## How do I update /dev/mmcblock0boot0

The special eMMC boot partitions are updatable the same way as the main
partition. When I create .fw files for manufacturing, I create two targets, a
`complete` target that updates the main eMMC and a `bootloader` target that
updates `mmcblock0boot0`. The manufacturing script runs `fwup` twice: once for
the `complete` target and then again for the `bootloader` target.

Also, the `/dev/mmcblock0boot0` device is forced read-only by the kernel. To
unlock it, run:

```sh
echo 0 > /sys/block/mmcblk0boot0/force_ro
```

## What's the best way to identify firmware versions

fwup supports several ways:

1. Store the version in `meta-version`. This is usually the friendliest for
   end users.
2. Store the `git` hash in `meta-vcs-identifier`. This is good for developers.
3. Use the `fwup`-computed UUID that's available in `meta-uuid' and
   `${FWUP_META_UUID}`.

Of these, the third one is always available since version fwup `v1.2.1`. The
motivation behind it was to unambiguously know whether installed firmware
matches the desired firmware. Since it is computed, `.fw` files generated with
previous versions of fwup have UUIDs.

The first two options require the versions to be added to the `fwup.conf` file.
They are usually added using environment variables so that the version numbers
are not hardcoded.

## How do I get the firmware metadata formatted as JSON

Use `jq`!

```sh
$ fwup -m -i $FW_FILE | jq -n -R 'reduce inputs as $i ({}; . + ($i | (match("([^=]*)=\"(.*)\"") | .captures | {(.[0].string) : .[1].string})))'
{
  "meta-product": "My Awesome Product",
  "meta-version": "0.1.0",
  "meta-author": "All of us",
  "meta-platform": "imx6",
  "meta-architecture": "arm",
  "meta-creation-date": "2018-11-07T14:46:38Z",
  "meta-uuid": "7add3c6d-230c-5bf1-77ec-5f785e91be40"
}
```

## How do I use "raw" NAND Flash

Some "raw" NAND Flash requires a wear leveling layer such as UBI.  See
the [UBI Example fwup.conf](docs/ubi_example/fwup.conf) for how to integrate
fwup with the [UBI toolchain](http://www.linux-mtd.infradead.org/doc/ubi.html).

## How do you pronounce fwup

I used to pronounce it "eff-double-you-up", but then coworkers and others
started calling it "fwup" (one syllable) and "fwup-dates" when referring to the
`.fw` files. I now use the one syllable version. This has caused issues in the
documentation where "an" is used rather than "a". Feel free to send PRs.

# Licenses

This utility contains source code with various licenses. The bulk of the code is
licensed with the Apache-2.0 license which can be found in the `LICENSE` file.

All statically-linked 3rd party source code can be found in the `src/3rdparty`
directory. The following sections summarize the included code licenses.

## base64.c

Public Domain or Creative Commons CC0. See file for explanatory text.

## FatFS

The FAT filesystem code (FatFs) comes from [elm-chan.org](http://elm-chan.org/fsw/ff/00index_e.html)
and has the following license:

>>>
FatFs module is a generic FAT file system module for small embedded systems.
This is a free software that opened for education, research and commercial
developments under license policy of following terms.

 Copyright (C) 2015, ChaN, all right reserved.

* The FatFs module is a free software and there is NO WARRANTY.
* No restriction on use. You can use, modify and redistribute it for
  personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
* Redistributions of source code must retain the above copyright notice.
>>>

## Monocypher

This package has been dual licensed under the 2-clause BSD and CC-0. See
[LICENCE.md](src/3rdparty/monocypher-3.0.0/LICENCE.md).

## semver.c

`fwup` uses [semver.c](https://github.com/h2non/semver.c) for checking versions.
`semver.c` is Copyright (c) Tomás Aparicio and distributed under the MIT
License. See [LICENSE](src/3rdparty/semver.c/LICENSE).

## strptime.c

On systems without the function strptime(), a version from Google is
included that is distributed under the Apache 2.0 license.

## Tiny AES

This code was released into the public domain. See
[unlicense.txt](src/3rdparty/tiny-AES-c/unlicense.txt) for details.

## Xdelta3

Only the xdelta3 decompressor is currently used by `fwup` so most of the code in
the `xdelta3` directory is ignored or disabled. Importantly, `fwup`'s use of
xdelta3 does not bring in any xdelta3 dependencies. Xdelta3 is covered by the
Apache-2.0 license.
