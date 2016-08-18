#!/bin/bash

#
# Build script for Travis
#
# Inputs:
#    TRAVIS_OS_NAME - "linux" or "osx"
#    MODE           - "static", "dynamic", or "windows"
#
# Static builds use scripts to download libarchive, libconfuse, and libsodium,
# so those are only installed on shared library builds.
#

set -e
set -v

# Create ./configure
./autogen.sh

case "${TRAVIS_OS_NAME}-${MODE}" in
    *-static)
        # If this is a static build, run 'build_pkg.sh'
        bash -v scripts/build_pkg.sh
        exit 0
        ;;
    linux-dynamic)
        ./configure --enable-gcov
        ;;
    osx-dynamic)
        PKG_CONFIG_PATH="/usr/local/opt/libarchive/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure;
        ;;
    linux-windows)
        export CROSS_COMPILE=x86_64-w64-mingw32
        export CC=$CROSS_COMPILE-gcc
        bash -v scripts/build_pkg.sh
        exit 0
        ;;
    *)
        echo "Unexpected build option: ${TRAVIS_OS_NAME}-${MODE}"
        exit 1
esac


# Normal build
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


