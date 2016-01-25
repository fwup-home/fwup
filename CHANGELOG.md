# Changelog

## v0.5.1-dev

  * Bug fixes
    * Fix rpath issue with libsodium not being in a system library directory

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
