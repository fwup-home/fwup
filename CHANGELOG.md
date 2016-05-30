# Changelog

## v0.7.0-dev

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
