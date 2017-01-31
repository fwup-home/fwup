# Changelog

## v0.14.0-dev

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
    * Updated FatFs from R0.11a to R0.12b. See
      http://elm-chan.org/fsw/ff/updates.txt.

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
