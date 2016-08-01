#!/bin/bash

#
# Build script for Travis
#
# Inputs:
#    TRAVIS_OS_NAME - "linux" or "osx"
#    BUILD_STATIC   - "true" or "false"
#
# Static builds use scripts to download libarchive, libconfuse, and libsodium,
# so those are only installed on shared library builds.
#

set -e
set -v

# Create ./configure
./autogen.sh

# If this is a static build, run 'build_pkg.sh'
if [[ "$BUILD_STATIC" = "true" ]]; then
    if [[ "$TRAVIS_OS_NAME" = "osx" ]]; then
        export SKIP_PACKAGE=true
    fi
    bash -v scripts/build_pkg.sh
    exit 0
fi

# Normal build
if [ "$TRAVIS_OS_NAME" = "linux" ]; then
    ./configure --enable-gcov
else
    PKG_CONFIG_PATH="/usr/local/opt/libarchive/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure;
fi
make
make check
make dist

# Check that the distribution version works by building it again
tar xf fwup-*.tar.gz
cd fwup-*
if [ "$TRAVIS_OS_NAME" = "linux" ]; then
    ./configure;
else
    PKG_CONFIG_PATH="/usr/local/opt/libarchive/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure;
fi
make
make check


