# Changelog

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
