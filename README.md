![The fwup pup](docs/fwup-pup.png)

# Overview
[![Build Status](https://travis-ci.org/fhunleth/fwup.svg?branch=master)](https://travis-ci.org/fhunleth/fwup)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/4094/badge.svg)](https://scan.coverity.com/projects/4094)
[![Coverage Status](https://coveralls.io/repos/github/fhunleth/fwup/badge.svg)](https://coveralls.io/github/fhunleth/fwup)

The `fwup` utility is a configurable image-based firmware update utility for
embedded Linux-based systems. It has two modes of operation. The first mode
creates compressed archives containing root file system images, bootloaders, and
other image material. These can be distributed via websites, email or update
servers. The second mode applies the firmware images in a robust and repeatable
way. The utility has the following features:

  1. Uses standard ZIP archives to make debugging and transmission simple.

  2. Simple, but flexible configuration language to enable firmware
  updates on various platforms and firmware update policies.

  3. Streaming firmware update processing to simplify target storage requirements.

  4. Multiple firmware update task options per archive so that one
  archive can upgrade varying target configurations

  5. Basic disk partitioning and FAT filesystem manipulation

  6. Human and machine readable progress.

  7. Initialize or update SDCards on your development system whether you're
     running Linux, OSX, or Windows. MMC and SDCards are automatically
     detected and unmounted. No need to scan logs or manually unmount.

  8. Firmware archive digital signature creation and verification (BETA!!)

  9. Permissive license (Apache 2.0 License - see end of doc)

This utility is based off of firmware update utilities I've written for
various projects. It has already received a lot of use with the open source
Nerves Project and other embedded projects.

# Examples!

See [bbb-buildroot-fwup](https://github.com/fhunleth/bbb-buildroot-fwup) for firmware update examples
for the BeagleBone Black and Raspberry Pi. The [Nerves Project](https://github.com/nerves-project/nerves_system_br)
has more examples and is better maintained. The regression tests can also be helpful.

My real world use of `fwup` involves writing
the new firmware to a place on the Flash that's not in current use and then
'flipping' over to it at the very end. The examples tend to reflect that.
`fwup` can also be used to overwrite
an installation in place assuming you're using an initramfs, but that doesn't
give protection against someone pulling power at a bad time. Also, `fwup`'s one pass
over the archive feature means that firmware validation is mostly done on the fly,
so you'll want to verify the archive first (see the `-V` option).

# Installing

The simplest way to install `fwup` is via a package manager or installer.

On OSX, `fwup` is in [homebrew](http://brew.sh/):

    brew install fwup

On Linux, download and install the appropriate package for your platform:

  * [Debian/Ubuntu AMD64 .deb](https://github.com/fhunleth/fwup/releases/download/v0.9.1/fwup_0.9.1_amd64.deb)
  * [Raspbian armhf .deb](https://github.com/fhunleth/fwup/releases/download/v0.9.1/fwup_0.9.1_armhf.deb)
  * [RedHat/CentOS x86\_64 .rpm](https://github.com/fhunleth/fwup/releases/download/v0.9.1/fwup-0.9.1-1.x86_64.rpm)
  * Arch Linux - See [fwup package](https://aur.archlinux.org/packages/fwup-git/) on AUR

On Windows, `fwup` can be installed from [chocolatey](http://chocolatey.org)

   choco install fwup

Alternatively, download the [fwup executable](https://github.com/fhunleth/fwup/releases/download/v0.9.1/fwup.exe)
and place it in your path.

If you're using another platform or prefer to build it yourself, download the
latest [source code release](https://github.com/fhunleth/fwup/releases/download/v0.9.1/fwup-0.9.1.tar.gz) or clone this repository. Then read one of the following files:

  * [Linux build instructions](docs/build_linux.md)
  * [OSX build instructions](docs/build_osx.md)
  * [Windows build instructions](docs/build_windows.md)
  * [Raspbian build instructions](docs/build_rpi.md)

When building from source, please verify that the regression test pass
on your system (run `make check`) before using `fwup` in production. While
the tests usually pass, they have found minor issues in third party libraries in the
past that really should be fixed.

# Invoking

```
Usage: fwup [OPTION]...

Options:
  -a, --apply   Apply the firmware update
  -c, --create  Create the firmware update
  -d <file> Device file for the memory card
  -D, --detect List attached SDCards or MMC devices
  -E, --eject Eject removeable media after successfully writing firmware.
  --no-eject Do not eject media after writing firmware
  -f <fwupdate.conf> Specify the firmware update configuration file
  -F, --framing Apply framing on stdin/stdout
  -g, --gen-keys Generate firmware signing keys (fwup-key.pub and fwup-key.priv)
  -i <input.fw> Specify the input firmware update file (Use - for stdin)
  -l, --list   List the available tasks in a firmware update
  -m, --metadata   Print metadata in the firmware update
  -n   Report numeric progress
  -o <output.fw> Specify the output file when creating an update (Use - for stdout)
  -p <keyfile> A public key file for verifying firmware updates
  -q, --quiet   Quiet
  -s <keyfile> A private key file for signing firmware updates
  -S, --sign Sign an existing firmware file (specify -i and -o)
  -t, --task <task> Task to apply within the firmware update
  -u, --unmount Unmount all partitions on device first
  -U, --no-unmount Do not try to unmount partitions on device
  -v, --verbose   Verbose
  -V, --verify  Verify an existing firmware file (specify -i)
  --version Print out the version
  -y   Accept automatically found memory card when applying a firmware update
  -z   Print the memory card that would be automatically detected and exit

Examples:

Create a firmware update archive:

  $ fwup -c -f fwupdate.conf -o myfirmware.fw

Apply the firmware to an attached SDCard. This would normally be run on the host
where it would auto-detect an SDCard and initalize it using the 'complete' task:

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

To define a constant that is not affected by environment variables of the same name, use `define!`:

    define!(MY_CONSTANT, "Can't override this")


## Global scope

At the global scope, the following options are available:

Key                  | Description
---------------------|------------
require-fwup-version | Require a minimum version of fwup to apply this update (currently informational only)
meta-product         | Product name
meta-description     | Description of product or firmware update
meta-version         | Firmware update version
meta-author          | Author or company behind the update
meta-platform        | Platform that this update runs on (e.g., rpi or bbb)
meta-architecture    | Platform architectures (e.g., arm)
meta-creation-date   | Timestamp when the update was created (automatically added). If you want to force the timestamp, set the `NOW` environment variable.
meta-fwup-version    | Version of fwup used to create the update (automatically added)

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
the update configuration. `fwup` will automatically record the length and BLAKE2b-256 hash of the
file in the archive. These fields are used internally to compute progress and verify the contents
of the archive. A typical `file-resource` section looks like this:

```
file-resource zImage {
        host-path = "output/images/zImage"
}
```

Resources are usually stored in the `data` directory of the firmware archive.
This is transparent for most users. If you need to make the `.fw` file
interoperate with other software, it is sometimes useful to embed a file into
the archive at another location. This can be done by specifying an absolute path
resource as follows:

```
file-resource "/my_custom_metadata" {
        host-path = "path/to/my_custom_metadata_file"
}
```

### Resource concatenation

Sometimes you need to concatenate multiple files together to form one `file-resource`. While
you can sometimes do this using multiple calls to `raw_write`, that won't work if you don't
know the file offsets a priori or the offsets don't fall on block boundaries. Another
alternative is to concatenate files as a prep step to fwup. If that's inconvenient, `fwup`
allows multiple paths to be specified in `host-path` that are separated by semicolons.
They will be concatenated in the order they appear.

```
file-resource kernel_and_rootfs {
        # Concatenate uImage and the rootfs. OpenWRT mtd splitter will
        # separate them back out at runtime.
        host-path = "output/images/uImage;output/images/rootfs.squashfs"
}
```

### File resource validation checks

When creating archives, `fwup` can perform validation checking on file resources to catch
simple errors. These checks can catch common errors like file resources growing too large
to fit on the destination or files truncated due to cancelled builds.

Note that these checks are not performed when applying updates, since the actual
length (and a hash) is recorded in the archive metadata and used for verification.

The following checks are supported:

Check           | Description
----------------|------------
assert-size-lte | If the file size is not less than or equal the specified amount, report an error.
assert-size-gte | If the file size is not greater than or equal the specified amount, report an error.

Sizes are given in 512 byte blocks (like everything else in `fwup`).

Here's an example:

```
file-resource rootfs.img {
        host-path = "output/images/rootfs.squashfs"
        assert-size-lte = ${ROOTFS_A_PART_COUNT}
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
        bootstrap-code-host-path = "path/to/bootstrap-data" # should be 440 bytes

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

If you're using an Intel Edison or similar platform, `fwup` supports generation of the OSIP
header in the MBR. This header provides information for where to load the bootloader (e.g.., U-Boot)
in memory. The `include-osip` option controls whether the header is generated. The OSIP and
image record (OSII) option names map directly to the header fields with the exception that
length, checksum and image count fields are automatically calculated. The following is an
example that shows all of the options:

```
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

Constraint                                         | Description
---------------------------------------------------|------------
require-partition-offset(partition, block_offset)  | Require that the block offset of a partition be the specified value
require-fat-file-exists(block_offset, filename)    | Require that a file exists in the specified FAT filesystem

*More constraints to be added as needed*

The remainder of the `task` section is a list of event handlers. Event handlers are
organized as scopes. An event handler matches during the application of a firmware update
when an event occurs. Events include initialization, completion, errors, and files being
decompressed from the archive. Since archives are processed in a streaming manner, the
order of events is deterministic based on the order that files were added to the archive.
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
fat_attrib(block_offset, filename, attrib) | Modify a file's attributes. attrib is a string like "RHS" where R=readonly, H=hidden, S=system
fat_mv(block_offset, oldname, newname) | Rename the specified file on a FAT file system
fat_rm(block_offset, filename)        | Delete the specified file
fat_mkdir(block_offset, filename)     | Create a directory on a FAT file system
fat_setlabel(block_offset, label)     | Set the volume label on a FAT file system
fat_touch(block_offset, filename)     | Create an empty file if the file doesn't exist (no timestamp update like on Linux)
fw_create(fwpath)                     | Create a firmware update archive in the specified place on the target (e.g., /tmp/on-reboot.fw)
fw_add_local_file(fwpath, name, local_path) | Add the specified local file to a firmware archive as the resource "name"

# Firmware authentication

Firmware archives can be authenticated using a simple public/private key scheme. To
get started, create a public/private key pair by invoking `fwup -g`. The algorithm
used is [Ed25519](http://ed25519.cr.yp.to/). This generates two file: `fwup-key.pub`
and `fwup-key.priv`. It is critical to keep the signing key, `fwup-key.priv` secret.

To sign an archive, pass `-s fwup-key.priv` to fwup when creating the firmware. The
other option is to sign the firmware archive after creation with `--sign` or `-S`.

To verify that an archive has been signed, pass `-p fwup-key.pub` on the command line
to any of the commands that read the archive. E.g., `-a`, `-l` or `-m`.

It is important to understand how verification works so that the security of the
archive isn't compromised. Firmware updates are applied in one pass to avoid needing
a lot of memory or disk space. The consequence of this is that verification is
done on the fly. The main metadata for the archive is always verified before any
operations occur. Cryptographic hashs (using the [BLAKE2b-256](https://blake2.net/) algorithm) of each
file contained in the archive is stored in the metadata. The hash for each file
is computed on the fly, so a compromised file may not be detected until it has
been written to Flash. Since this is obviously bad, the strategy for creating
firmware updates is to write them to an unused location first and then switch
over at the last possible minute. This is desirable to do anyway, since this strategy
also provides some protection against the user disconnecting power midway through
the firmware update.

# Integration with applications

It is expected that many users will want to integrate `fwup` with their applications.
Many operations can be accomplished by just invoking the `fwup` executable and parsing
the text written to `stdout`. When applying firmware progress updates are delivered
based on commandline options:

  1. Human readable - This is the default. Progress is updated from the text `0%` to `100%`.
  2. Numeric (`-n`) - Progess is printed as `0\n` to `100\n`
  3. Quiet (`-q`) - No progress is printed

While the above works well for scripts and when errors can be seen by the operator, `fwup`
supports a structured use of `stdin`/`stdout` as well. Specify the `--framing` option to
any of the commands to use this option.

The framing feature is influenced by the Erlang VM's port API and should be relatively
easy to integrate with non-Erlang VM languages. The framing works around deficiencies
in the built-in interprocess communication. For example, by enabling framing, a program
can stream a firmware update through `fwup's` `stdin` without needing to close its
`stdout` to signal end of file. Another feature aided by framing is knowing what text
goes together and whether the text is part of an error message or not. Exit status is
still an indicator of success or failure, but the controlling application doesn't
need to wait for the program to exit to know what happened.

In `--framing` mode, all communication with `fwup`
is done in packets (rather than byte streams). A packet starts with a 4 byte length field.
The length is a big endian (network byte order) unsigned integer. A zero-length packet
(i.e., 4 bytes of zeros) signals end of input.

Field          | Size         | Description
---------------|--------------|-------------
Length         | 4 bytes      | Packet length as a big endian integer
Data           | Length bytes | Payload

Input and output packets have different formats. For sending input to `fwup` (like when
streaming a `.fw` file using stdio), the input bytes should be framed into packets
however is most convenient. For example, if bytes are received in 4K chunks, then they
can be sent to `fwup` in 4K packets with a zero-length packet at the end. The packets
need not be the same size.

All output packets from `fwup` have a 2 byte type field at the
beginning of the packet:

Field          | Size           | Description
---------------|----------------|-------------
Length         | 4 bytes        | Packet length as a big endian integer
Type           | 2 bytes        | See below
Data           | Length-2 bytes | Payload

The following types are defined:

Type           | 2 byte value | Description
---------------|--------------|------------
Success        | "OK"         | The command was executed successfully. The payload contains the result.
Error          | "ER"         | A failure occurred. The payload is a 2 byte error code (future use) followed by a textual error message.
Warning        | "WN"         | A warning occurred. The payload is a 2 byte error code (future use) followed by a textual error message.
Progress       | "PR"         | The next two bytes are the progress (0-100) as a big endian integer.

# FAQ

## How do I include a file in the archive that isn't used by fwup?

The scenario is that you need to store some metadata or some other information
that is useful to another program. For example, you'd like to include some
documentation or other notes inside the firmware update archive that the
destination device can pull out and present on a UI. To do this, just add
`file-resource` blocks for each file. These blocks don't need to be referenced
by an `on-resource` block.

## How do I include the firmware version in the archive?

If you are using git, you can invoke `fwup` as follows:

    GITDESCRIBE=`git describe` fwup -c -f myupdate.conf -o out.fw

Then in `myupdate.conf` add the line:

    meta-version = "${GITDESCRIBE}"

On the target device, you can retrieve the version by using `-m`. For example:

    $ fwup -m -i out.fw
    meta-product = "Awesome product"
    meta-description = "A description"
    meta-version = "v0.0.1"
    meta-author = "Me"
    meta-platform = "bbb"
    meta-architecture = "arm"
    meta-creation-date = "2014-09-07T19:50:57Z"

## How do I get the best performance?

In general, `fwup` writes to Flash memory in large blocks so that
the update can occur quickly. Obviously, reducing the amount that needs to get
written always helps. Beyond that, most optimizations are platform dependent.
Linux caches writes so aggressively that writes to Flash memory are nearly as
fast as possible. OSX, on the other hand, does very little caching, so doing
things like only working with one FAT filesystem at a time can help. In this
case, `fwup` only caches writes to one FAT filesystem at a time, so mixing them
will flush caches. OSX is also slow to unmount disks, so keep in mind that
performance can only be so fast on some systems.

# Licenses

This utility contains source code with various licenses. The bulk of the code is
licensed with the Apache 2.0 license which can be found in the `LICENSE` file.

All 3rd party source code can be found in the `3rdparty` directory.

The FAT filesystem code (FatFs) comes from http://elm-chan.org/fsw/ff/00index_e.html
and has the following license:

```
FatFs module is a generic FAT file system module for small embedded systems.
This is a free software that opened for education, research and commercial
developments under license policy of following terms.

 Copyright (C) 2015, ChaN, all right reserved.

* The FatFs module is a free software and there is NO WARRANTY.
* No restriction on use. You can use, modify and redistribute it for
  personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
* Redistributions of source code must retain the above copyright notice.
```

On systems without the function strptime(), a version from Google is
included that is distributed under the Apache 2.0 license.
