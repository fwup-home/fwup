# Changelog

## v1.8.3

This release updates FatFS (the FAT filesystem library) to the latest patch
release, R0.14a.

It also updates how fwup calls into the library to reduce the time window where
fwup can be responsible for FAT filesystem corruption. The incident that
prompted this change was a device that had repeated firmware update
interruptions had accumulated enough orphaned FAT clusters to fill the partition
and prevent an update. `fsck.fat` can fix this easily, but it's better if this
doesn't happen and this update should reduce the probability.

## v1.8.2

This release brings back and updates the Windows Chocolatey package.

## v1.8.1

This release only contains meaningful changes for the host side. There is no
need to upgrade devices.

* Bug fixes
  * Improved automatic detection of SD Cards on Linux and OSX. This includes
    checks for whether they're writable, removable and non-virtual on Mac so
    that a number of false positive drives can be removed from the list.
  * Remove arbitrary 64 GB max SD card size for automatic detection for OSX and
    Linux. If you have a 128 GB or larger SD Card, you no longer have to force
    it to be selected. The previous change makes this behavior less risky.

## v1.8.0

* New features
  * U-Boot redundant environment support - Thanks to Davide Pianca, `fwup` can
    read and update devices with redundant U-Boot environments. Add the
    `block-offset-redund` to your fwup.conf U-Boot environment specification to
    enable and use.

* Bug fixes
  * Fixed memory leaks in archive verification and when processing empty
    `fwup.conf` files. Neither of these leaks were in code paths that would
    normally be exercised on devices.

## v1.7.0

In this release, `fwup` will verify writes to raw devices. Regular files are not
verified unless forced with the `--verify-writes` option. Writes are verified as
they're written. Since the final write with `fwup` firmware updates is usually
to toggle the active partition, corruption can fail an update before the toggle
occurs.

* New features
  * Writes verification on raw devices. This may be forced with
    `--verify-writes` or disabled with `--no-verify-writes`.
  * The `boot` option works for GPT partitions now. `boot=true` will set the GPT
    attribute flags appropriately. Since the archive creation process replaces
    the `boot` parameter with raw flags, older versions of `fwup` can apply
    updates that use this.

* Bug fixes
  * After a write error was detected, `fwup` would continue to try to write a
    few more blocks before exiting. Now it no longer writes after an error. An
    `on-error` handle can perform a write, but it will start from a clean slate
    now.

## v1.6.0

This release adds beta support for VCDIFF delta updates. This makes it possible
to significantly reduce firmware update sizes if you know the firmware version
running on the device. See the README.md for details.

IMPORTANT: If you were using `fwup`'s sparse file support, the `skip-holes`
option now defaults to `false`. You must set `skip-holes=true` for any resources
that should be scanned. Since this is an optimization, nothing should break, but
it will be slower. This will affect firmware that writes large and empty ext2/4
filesystems (and similar). Most known `fwup` usage is unaffected. The default
was changed since `skip-holes` could cause confusion and it was incompatible
with delta updates.

* Improvements
  * VCDIFF support using xdelta3
  * Progress bar now shows bytes in
  * Updated to FatFS R0.14

* Bug fixes
  * Refactor firmware archive validation logic to more closely match firmware
    update code to avoid false failures due to differences in ZIP file
    processing

## v1.5.2

This release does not change any functionality. It provides a modest (~30%)
reduction in fwup's footprint by trimming easy-to-remove code.

* Support a minimal "apply-only" of `fwup`. Use `./configure
  --enable-minimal-build` to select this.
* Replace libsodium with [Monocypher](https://monocypher.org/). Monocypher is
  a small crypto library that's very similar to libsodium in many ways, but
  optimizes for code size rather than performance.

Note that the regression tests will fail with the minimal version of `fwup`
since almost all tests create `.fw` archives and the minimal one can't do that.
It's possible to use a full-featured version of `fwup` to handle the creation
parts only so that the minimal `fwup` can be tested on everything else. See the
CI scripts if you'd like to replicate these tests.

## v1.5.1

* Improvements
  * Check exact size when decoding hex strings for MBR bootcode blocks. This
    fixes an error that makes a typo really hard to see.
  * Support hex strings for encrypted filesystem secrets to match dm-crypt.

## v1.5.0

* Bug fixes
  * Fixed overrun when writing to devices with non-128KB sizes. Fwup tries to
    write in 128KB blocks for efficiency and in an attempted to avoid
    read/modify/write operations for partial blocks on Flash-based devices. The
    code for this could write past the end of a device and this became more
    apparent with GPT support which writes partition information to the end of a
    device.

* New feature
  * Encrypted partition support for use with dm-crypt. This functionality should
    be considered "beta". Only the simplest encryption method (aes-cbc-plain)
    is available. This works, but requires some knowledge about working with
    encrypted root filesystems (the likely scenario for this feature).

## v1.4.0

* New feature
  * GPT partitions. This is perhaps the oldest feature request for fwup. Thanks
    for your patience. The support in this version allows for static GPT
    partitions and ones where the last partition grows to available space. All
    UUIDs must be specified (but you can use variables to inject dynamic UUIDs
    from scripts calling fwup. See the new `gpt_write` function and details in
    the README.md.

## v1.3.2

* Improvements
  * Support `SOURCE_DATE_EPOCH` for reproducible archive creation. See
    https://reproducible-builds.org/ for motivation and details. Similar support
    was available by setting the `NOW` environment variable, but this makes
    `fwup` follow the conventions used by pretty much everyone else.

## v1.3.1

* Bug fixes
  * Fix partition expansion on OSX.
  * Update `img2fwup` script to only require a generic `sh` instead of `bash`.
    Also support ` ./configure --disable-scripts` to disable installation of
    `img2fwup` completely. It isn't needed in embedded systems.

## v1.3.0

* New feature
  * Add `expand = true` option to the last partition in the MBR. When set, the
    partition size will grow to fill the remainder of the destination. The
    specified `block-count` is the minimum size and will be used if the
    destination's size can't be determined. This makes it easier to have
    application data partitions that resize based on installed memory.

* Updates
  * FatFS R0.13c (upgrade from R0.13a)

## v1.2.7

* Bug fixes
  * Implement eject on Windows to avoid automount after burn
  * Update libconfuse to pull in security fix (affects static builds only)

## v1.2.6

* Bug fixes
  * Fix missing include on some platforms
  * Limit size of autodetected removable drives on OSX to avoid Time Machine
    backup drives from being reported

## v1.2.5

* Bug fixes
  * When creating .fw files, include() only worked at the root level. It's now
    possible to include files in most places. Thanks to @mobileoverlord for
    reporting this.
  * When generating keys, it's now possible to specify a prefix path for where
    to put the outputs.
  * Error message improvements

## v1.2.4

* Bug fixes
  * Fixed an file truncation when writing to a FAT file partition and then
    switch to write a file to a different FAT partion. see
    https://github.com/fwup-home/fwup/pull/89. Thanks to @mikaelrobomagi for
    finding and fixing this.

## v1.2.3

* Bug fixes
  * Fail when writing to read-only regular files as root. This is almost
    certainly accidental. This also fixes an issue when packaging fwup with
    Alpine due to it running the regression tests in a fakeroot environment.

## v1.2.2

* Bug fixes
  * On OSX, ignore mounted .dmg files when listing MicroSD locations
  * Fix broken regression test on Alpine (the problem was with the test)

## v1.2.1

* Bug fixes
  * Clean up some ways of overiding the meta-uuid calculation
  * Export the FWUP_META_UUID variable so that the meta-uuid field is available
    to scripts

## v1.2.0

* New features
  * Introduce meta-uuid. This is an automatically generated metadata field that
    uniquely identifies a .fw file. It won't change if the .fw is rebuilt later
    (timestamp or compression level changes) or digitally signed.
  * Removed metadata that could change between creating .fw files so that it is
    easier to verify that nothing significant has changes. meta-creation-date is
    now automatically computed using zip file metadata and fwup-version has
    been deprecated in favor of using require-fwup-version if this info is
    important.

* Bug fixes
  * Don't try to be smart and skip holes with `img2fwup`. The hole detection
    feature is tricky to get right and having it default to on was an oversight
    with `img2fwup`. This release changes that.

## v1.1.0

* New features
  * Support passing more than one public key for archive verification
  * Support passing fwup configs in via stdin
  * Add img2fwup script to make it easier to create .fw archives from
    regular SDCard image files

* Bug fixes
  * Fixed framed mode output for archive verifcation and a couple other
    commands.
  * Bumped fatfs version from 0.12b to 0.13a. Changes appear to be minor in
    regards to fwup usage.
  * Regression tests failed if a file in /dev wasn't accessible. This was due to
    a call to `find` and the inaccessible files could be safely skipped.

## v1.0.0

* Bug fixes
  * Add include due to change in new versions of glibc
  * Disable failing test on Arch Linux (appears to be due to root filesystem
    options and not specifically Arch. Luckily test is irrelevent to archive
    creation which is what is how fwup is used on Arch.)

## v0.19.0

* New features
  * Implement `require-fwup-version` so .conf files that require newer
    versions of fwup can force a nicer error message.
  * Add `require-path-at-offset` to support matching off the block offset of a
    partition. This is intended to be used with `require-path-on-device` to
    figure out the actively running partition.

* Bug fixes
  * Fix relative path inclusion to be relative to the .conf file rather than
    relative to the current working directory.

## v0.18.1

* Bug fixes
  * Fix an error in the syscall validator that was triggered by glibc 2.26.
    This cause many unit tests to incorrectly fail.
  * Reverted Windows fwup script that broke the Chocolatey package

## v0.18.0

* New features
  * Added FWUP_SIZE_<resource_name> feature to support use in UBI systems.
    Thanks to Michael Schmidt for this feature.

* Bug fixes
  * Fixed undefined use of pthreads. OpenBSD caught this and some other issues.
  * Fixed regression issues when run on the new APFS in OSX High Sierra.
  * Reduced progress bar width to avoid rendering issues on thin terminals.

## v0.17.0

* New features
  * Added `--exit-handshake` to reduce code needed to integrate with Erlang
    and Elixir programs.

* Bug fixes
  * Fixed the TRIM amount for manual trim() requests
  * Don't close stdin early when streaming. This would cause an EPIPE in
    programs that weren't expecting the pipe to half close. Previously, stdin
    would be closed when done to workaround libarchive draining all input. A
    different workaround is now in place.
  * Make the progress bar 64-bit safe.
  * Add out of bounds checks to trim() and memset() to catch a couple issues
    at creation time (included uninitialized variables).
  * Cleaned up old progress bar trails when updates printed out information.

## v0.16.1

* New features
  * Added support for setting the signature field in the MBR

## v0.16.0

* New features
  * Added path_write, pipe_write, and execute commands. These are only usable
    if the --unsafe flag is passed. They enable people using raw NAND to
    invoke the ubi tools. People wanting fwup to upgrade other chips can also
    use these commands. These commands are considered experimental, can create
    platform-dependent .fw files, and open up some security issues. However,
    assuming signed firmware updates, they can be very useful. Thanks to
    Michael Schmidt for these additions.
  * Progress bar now includes approximate bytes written.

* Bug fixes
  * Support overwriting files in FAT partitions. Previously you had to remove
    the files first.
  * Fix tests to run on BSD systems again.

## v0.15.4

* New features
  * Changed signing keys to be base64 encoded so that they'd be easier to bake
    into firmware and pass in environment variables on CI systems. The
    previous raw binary format still works and will remain supported.
  * Added commandline parameters for passing public and private keys via
    commandline arguments. Along with the base64 change, this cleans up CI
    build scripts.

* Bug fixes/Improvements
  * Fix lseek seek_end issue on Mac when working with SDCards. This fixed an
    issue where upgrade tasks didn't work on Macs. Not a common issue, but
    confusing since you'd hit it while debugging.
  * Make requirement checks report their result with `-v`.
  * Fix verbose prints to use the fwup_warn helper instead of calling fprintf
    directely. (Cleanup)
  * Enlarged trim cache for up to 64 GiB memory devices. Large ones will work,
    but trim caching is ignored after 64 GiB. This should support almost all
    known uses of fwup now. The use of fwup on large SSDs still works, since
    fwup is pretty much only used at lower offsets.

## v0.15.3

* Bug fixes/Improvements
  * Fix segfault when using large media. This was found on a 1 TB SSD, but
    should have affected much smaller media.
  * Improved error messages for when FAT filesystems get corrupt and start
    returning weird errors.
  * Fixed trimming on media that wasn't a multiple of 128K bytes. This could
    have resulted in loss of data if anything was stored in the final bytes.
  * Fixed memory leaks identified by valgrind (nothing affecting proper
    operation)

## v0.15.2

* New features
  * Added meta-misc and meta-vcs-identifier metadata fields. This addition is
    backwards compatible assuming you're using libconfuse 3.0 or later. If you
    don't use these metadata fields in your fwup.conf files, there is no
    compatibility issue.

* Bug fixes/Improvements
  * Order block cache flush logic so that blocks get written in fwup.conf
    order. This fixes an issue where the cache could write the A/B partition
    swap before the new firmware was completely written. Given that the
    cache is pretty small and there was an extra flush before on-final,
    systems without the fix are likely fine.
  * Improve the caching heuristics to reduce the number of writes to FAT
    filesystems.
  * Improve detection of typos in variable names and content. This catches
    accidental writes to offset 0 when creating fwup.config files among other
    annoyances.
  * Improve MBR error messages

## v0.15.1

* Bug fixes
  * The OSX eject bug was finally found. It was due to the file handle still
    being open on the SDCard at the time of the eject. For whatever reason, it
    turned out to be 100% reproducable on v0.15.0 with a simple .fw file.

## v0.15.0

* Completely rewritten caching layer. This has the following improvements;
  * OS caching no longer used on Linux. Direct I/O is used now with the
    caching internal to fwup. For large archives, this results in about
    a 10% performance improvement. More importantly, fwup can provide more
    confidence that the final write to swap A/B partitions actually happens
    last.
  * Flash erase block aligned writes - currently hardcoded to 128 KB. This may
    be helpful for some devices even though it doesn't appear to improve
    performance.
  * One cache - previously there were limited ones for FAT operations and raw
    writes. This removed some code and the new implementation is simpler.
  * Support for discarding unused blocks and hardware TRIM
  * Unit tests now validate read and write syscalls for alignment, size and
    final order on Linux using ptrace.
  * The progress indicator now moves more linearly rather than racing to 99%
    and hanging for a while.

* New features
  * trim() command to discard storage regions. This avoids read/modify/write
    operations from being issued by fwup on smaller than erase-block sized
    writes.
  * `--enable-trim` option to enable the sending of hardware TRIM commands to
    devices that support them.
  * Added uboot_recover command for optionally re-initialize corrupt or
    uninitialized uboot environments. This is useful for adding u-boot
    environment blocks to devices without them as is being done with Nerves.

* Bug fixes
  * Fix segfault if a bad value is specified in the u-boot environment block.
  * Silence eject failure warning on OSX that appears to be harmless. Fixes
    #29.

## v0.14.3

* Bug fixes
  * Fix regression tests to run on ZFS. Thanks to georgewhewell for finding
    the issue and helping debug it.

## v0.14.2

* Bug fixes
  * Fixed fat_mkdir to not error if the directory already exists. This
    preserves pre-v0.14.0 expectations in some fwup.conf scripts. Other
    fat_mkdir errors will be reported and not ignored like in pre-v0.14.0
    days.
  * Changed warning code in framing to match specs. ("WA"->"WN"). Warnings
    are so rarely used that this was unnoticed.
  * Clarified framing docs

## v0.14.1

* Bug fixes
  * Patch libsodium 1.0.12 so that static mingw builds work.

## v0.14.0

* New features
  * Add support for creating resources inside configuration files. This makes
    it possible to create simple config files that have fwup variable
    references in them without requiring a separate script.
  * Add -1,-2,-3,...-9 to tune the compression. The default is -9, but on
    massive archives, this is really slow, so you can pass a lower number
    to speed up archive creation.
  * on-error has finally been implemented. This allows fwup.conf files to
    specify cleanup code should something go wrong. Cleanup is performed
    best effort and fwup still returns an error on exit.
  * Add support for matching strings in files on a FAT filesystem for whether
    to apply upgrades. See `require-fat-file-match()`. Useful for bootloaders
    that can modify a file, but not create/remove one (e.g., grub's grubenv
    file)
  * Verify that all on-resource handlers are run. This is the normal
    expectation, and this change guarantees it.

* Bug fixes
  * When streaming, the input would always be read through to the end. This
    meant that errors could take a long time to be reported and that was
    annoying at best.
  * FAT filesystem corruption wouldn't cause many of the fat_* commands to
    fail. They do now and unit tests have been added to verify this going
    forward.
  * Fix segfault when reading against a corrupt FAT filesystem due to a
    signed/unsigned conversion. fwup errors out properly now.

## v0.13.0

* New features
  * Add `require-path-on-device` as another way of detecting which partition
    is active when running A/B updates. This is only works when updating on
    device (the usual way of updating).
  * Add `info()` function for printing out information messages from updates.
    This is helpful when debugging scripts.

* Bug fixes
  * Flush messages in framed mode to prevent them from getting queued.
  * Send "OK" success message when done applying in framed mode.
  * Support writing >2GiB files on 32-bit platforms

## v0.12.1

* Bug fixes
  * Fix really subtle issue with archive_read_data_block returning 0 bytes
    when not at EOF. This issue should have been present since the beginning,
    but it only appeared recently.

## v0.12.0

* New features
  * Added easy way of invoking fwup for those just wanting to program an
    SDCard. Just run, "fwup myfile.fw" and fwup will automatically look
    for an attached SDCard, ask for confirmation, and apply the complete
    task.
  * Ported to FreeBSD and other BSDs. This allows .fw files to be created
    and applied on these systems. (I'm interested in hearing from embedded
    BSD users for refining this support.)
  * Added commandline options to scale the progress reports. See
    --progress-low and --progress-high
  * Added an error() function to let config files produce friendlier error
    messages when none of the upgrade (or any other) tasks match.

* Bug fixes
  * Sparse files that end with holes will be the right length when written.

## v0.11.0

* New features
  * Added sparse file support. This can significantly reduce the amount of
    time required to write a large EXT2 partition to Flash. It queries the
    OS for gaps so that it will write to the same places that mke2fs wrote
    and skip the places it didn't. See README.md for more info.
  * Updated FatFs from R0.11a to R0.12b

* Bug fixes
  * Several issues were found and fixed in the write caching code by
    constructing a special .fw file manually. To my knowledge, none of the
    issues could be triggered w/o sparse file support and a filesystem
    that supported <512 byte sparse blocks.

* Backwards incompatible changes
  * Sparse file support - if used, the created archives will not work on old
    versions of fwup.
  * Remove support for fw_create and fw_add_local_file. Neither of these were
    used much and there were better ways of accomplishing their functions.
    They became a pain to support, since they create .fw files that are
    missing sections, so they don't validate. It is much easier to run fwup
    to create multiple .fw files that are included in the master one for doing
    things like apply/commit or apply/revert style updates that stay within
    fwup. If you're using the functions, either don't upgrade or create new
    .fw configurings that embed pre-generated .fw files.

## v0.10.0

* New features
  * Add U-Boot environment support. This allows firmware updates to modify
    the U-Boot environment to indicate things like which partition is active.
    fwup can also look at the U-Boot environment to decide which partition to
    update.
  * Add raw_memset to write a fixed value over a range of blocks. This is good
    for invalidating SDCard regions in manufacturing so that they're
    guaranteed to be reinitialized on first boot. SDCard TRIM support would be
    better, but fwup doesn't work on the bulk programmers.
  * Add define_eval() to support running simple math expressions when building
    firmware update packages. This makes entering offset/size pairs less
    tedious.

* Bug fixes
  * Re-enable max SDCard size check on Linux to reduce risk of writing to an
    hard drive partition by accident.

## v0.9.2

* Bug fixes
  * Fix SDCard corruption issue on Windows. Disk volumes are now locked for
    the duration of the write process. Thanks to @michaelkschmidt for the fix.
  * Allow /dev/sda to be auto-detected as an SDCard on Linux. It turned out
    that for some systems, this was a legit location. Most people will not
    see it, since it doesn't pass other tests.
  * Set compression parameters on libarchive's zip compressor. This wasn't a
    actually a bug, but there seemed to be some variability in how .fw files
    were compressed.

## v0.9.1

* New features
  * Build Chocolatey package for Windows - lets Windows user run `choco
    install fwup` once the package is accepted into the Chocolatey repo.
  * Build a `.deb` package for Raspbian. This makes it easier to install
    `fwup` on Raspberry Pis. CI now builds and tests 32-bit armhf versions.

## v0.9.0

* New features
  * Windows port - It's now possible to natively write to SDCards on
    Windows.

* Bug fixes
  * Don't create files in `/dev`. This fixes a TOCTOU bug where an SDCard
    exists during enumeration time and disappears before the write. When this
    happened, a regular file was created in `/dev` which just confused
    everyone.
  * Support writing 0-byte files to FAT partitions. They were being skipped
    before. This is different than `touching` a file since it can be used to
    truncate an existing file.

## v0.8.2

This release has only one fix in it to address a corruption issue when updating
an existing FAT filesystem. The corruption manifests itself as zero'ing out 512 bytes
at some place in the filesystem that wasn't being upgraded. It appears that the
location stays the same with the configuration that reproduced the issue. Due to some luck,
the condition that causes this is relatively rare and appears to only have
caused crashes for BeagleBone Green users. BeagleBone Black users still had the
corruption, but it did not affect operation.

It is highly recommended to upgrade the version of `fwup` on your target to this
version.

* Bug fixes
  * Fix FAT filesystem corruption when running a software update.

## v0.8.1

This release is a bug fix release on v0.8.0. The combination of significantly
improved code coverage on the regression tests (see Coveralls status) and the
Windows port uncovered several bugs. People submitting fwup to distributions are
highly encouraged to run the regression tests (`make check`), since some issues
only appear when running against old or misconfigured versions of libconfuse and
libarchive.

* Bug fixes
  * Use pkg-config in autoconf scripts to properly discover transitive
    dependencies. Thanks to the Buildroot project for discovering a broken
    configuration.
  * Fix lack of compression support in libarchive in static builds (regression
    test added to catch this in the future)
  * Fix MBR partition handling on 32-bit systems (offsets between 2^31 and
    2^32 would fail)
  * Fix uninitialized variable when framing on stdin/stdout is enabled
  * Various error message improvements thanks to Greg Mefford.

## v0.8.0

* New features
  * Added assertions for verifying that inputs don't excede their expected
    sizes. The assertions are only checked at .fw creation time.
  * Add support for concatenating files together to create one resource. This
    was always possible before using a preprocessing script, but is more
    convenient now.
  * Add "framed" input and output when using stdin/stdout to simplify
    integration with Erlang and possibly other languages.
  * Detecting attached SDCards no longer requires superuser permissions on
    Linux.
  * Listing detected SDCards (`--detect` option) prints the SDCard size as
    well. ***This is a breaking change if you're using this in a script***

* Bug fixes
  * Fixed va_args bug that could cause a crash with long fwup.conf inputs
  * fwup compiles against uclibc now
  * autoreconf can be run without creating the m4 directory beforehand

## v0.7.0

This release introduces a change to specifying requirements on upgrade sections.
Previously, the only supported option was `require-partition1-offset`.
Requirements can now be specified using function syntax. If you have older versions of
`fwup` in the field, using this new feature will create .fw files that won't
apply. The change makes requirement support less of a hack. If you don't change
to the new syntax, `fwup` will continue to create `.fw` files that are
compatible with old versions.

* New features
  * Task requirement code now uses functions for checks
  * Add fat_touch to create 0 length files on FAT filesystems
  * Add require-fat-file-exists to check for the existance of a file when
    determining which task to run
  * libconfuse 3.0's unknown attribute support is now used to make fwup
    more robust against changes to meta.conf contents
  * open_memstream (or its 3rd party implementation) is no longer needed. This
    helps portability.
  * The meta.conf file is stripped of empty sections, lists, and attributes
    set to their defaults. If you have an old fwup version in the field, or
    you're using libconfuse < 3.0, it is now much harder to generate
    incompatible fwup files assuming you don't use features newer than your
    deployed fwup versions.

* Bug fixes
  * Autodetection will work for SDCards up to 64 GB now
  * Fixed off-by-month bug when creating files in FAT partitions

## v0.6.1

* New features
  * Add bash completion
  * Prebuilt .deb and .rpm archives for Linux

## v0.6.0

* Bug fixes
  * Fix FAT filesystem creation for AM335x processors (Beaglebone Black)

* New features
  * Added manpages

## v0.5.2

* Bug fixes
  * Clean up progress printout that came before SDCard confirmation
  * Improve error message when the read-only tab of an SDCard is on

## v0.5.1

* New features
  * sudo is no longer needed on OSX to write to SDCards
  * --unmount and --no-unmount commandline options to unmount (or not) all partitions on a device first
  * --eject and --no-eject commandline options to eject devices when complete (OSX)
  * --detect commandline option to list detected SDCards and removable media

* Bug fixes
  * Various installation fixes and clarifications in the README.md
  * Fix rpath issue with libsodium not being in a system library directory
  * If unmounting is needed and fails, fwup fails. This is different from before. Use --no-unmount to skip this step.

## v0.5.0

* New features
  * Add `--version` commandline option
  * Document and add more long options
  * Update fatfs library to R0.11a

* Bug fixes
  * Improve disk full error message (thanks Tony Arkles)

## v0.4.2

* New features
  * Write caching for FAT and large binaries (helps on OSX)

## v0.4.1

* Bug fixes
  * Fix streaming upgrades and add unit tests to catch regressions

## v0.4.0

* New features
  * Builds and programs SDCards on OSX

## v0.3.1

* Fixes
  * Fix segfault on some unknown arguments

## v0.3.0

This release uses libsodium for cryptographic routines. This makes it backwards
incompatible with v0.2.0 and earlier.

* New features
  * Firmware signing using Ed25519
  * Switch hash from SHA to BLAKE2b-256
  * Add support for OSIP headers (required for Intel Edison boards)

## v0.2.0

* New features
  * fat_mkdir and fat_setlabel support
