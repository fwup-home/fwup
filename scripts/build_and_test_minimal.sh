#!/bin/bash

#
# Build both full featured and minimal versions of fwup and test the minimal one
#

set -e
set -v

mkdir full_featured
mkdir minimal

cd minimal
../configure --enable-minimal-build
make -j4
cd ..

cd full_featured
../configure
make -j4
cd ..

# Go back and test the minimal version of fwup, but use the full-featured one
# to create the archives.
cd minimal
FWUP_CREATE=$PWD/../full_featured/src/fwup make -j4 check

