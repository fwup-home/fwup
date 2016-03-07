# Changelog

## v0.6.2-dev

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
