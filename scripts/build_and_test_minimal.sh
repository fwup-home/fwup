#!/bin/bash

#
# Build both full featured and minimal versions of fwup and test the minimal one
#

set -e
set -v

# Just in case the source directory has been configured, clean it up.
make distclean || true

# Create minimal and full-featured versions in separate build directories
mkdir minimal
cd minimal
../configure --enable-minimal-build
make -j4
cd ..

mkdir full_featured
cd full_featured
../configure
make -j4
cd ..

# Go back and test the minimal version of fwup, but use the full-featured one
# to create the archives.
cd minimal
FWUP_CREATE=$PWD/../full_featured/src/fwup make -j4 check

