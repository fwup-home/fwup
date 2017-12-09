#!/bin/bash

#
# Build script for CircleCi
#
# Inputs:
#    CIRCLE_OS_NAME - "linux" or "osx"
#    MODE           - "static", "dynamic", "windows", "raspberrypi", or "minimal"
#
# Static builds use scripts to download libarchive and libconfuse, so those are
# only installed on shared library builds.
#

set -e
set -v

FWUP_VERSION=$(cat VERSION)

# Create ./configure
./autogen.sh

case "${CIRCLE_OS_NAME}-${MODE}" in
    *-static)
        # If this is a static build, run 'build_pkg.sh'
        bash -v scripts/build_pkg.sh
        exit 0
        ;;
    linux-minimal)
        bash -v scripts/build_and_test_minimal.sh
        exit 0
        ;;
    linux-dynamic)
        ./configure --enable-gcov
        ;;
    linux-singlethread)
        # The verify-syscalls script can't follow threads, so
        # single thread builds are the only way to verify that
        # the issued read and write calls follow the expected
        # alignment, size and order
        ./configure --enable-gcov --without-pthreads
        ;;
    osx-dynamic)
        PKG_CONFIG_PATH="$(brew --prefix libarchive)/lib/pkgconfig:$(brew --prefix)/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure
        ;;
    linux-windows)
        CC=x86_64-w64-mingw32-gcc \
            CROSS_COMPILE=x86_64-w64-mingw32 \
            bash -v scripts/build_pkg.sh
        exit 0
        ;;
    linux-raspberrypi)
        CC=arm-linux-gnueabihf-gcc \
            CROSS_COMPILE=arm-linux-gnueabihf \
            PATH=~/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin:$PATH \
            QEMU_LD_PREFIX=~/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/arm-linux-gnueabihf/libc/lib/arm-linux-gnueabihf \
            bash -v scripts/build_pkg.sh
        exit 0
        ;;
    *)
        echo "Unexpected build option: ${CIRCLE_OS_NAME}-${MODE}"
        exit 1
esac

# Normal build
make -j4
if ! make -j4 check; then
    cat tests/test-suite.log
    echo "git source 'make check' failed. See log above"
    exit 1
fi
make dist

# Check that the distribution version works by building it again
mkdir distcheck
cd distcheck
tar xf ../fwup-$FWUP_VERSION.tar.gz
cd fwup-$FWUP_VERSION
if [ "$CIRCLE_OS_NAME" = "linux" ]; then
    ./configure;
else
    PKG_CONFIG_PATH="$(brew --prefix libarchive)/lib/pkgconfig:$(brew --prefix)/lib/pkgconfig:$PKG_CONFIG_PATH" ./configure
fi
make -j4
if ! make -j4 check; then
    cat tests/test-suite.log
    echo "Distribution 'make check' failed. See log above"
    exit 1
fi
cd ../..
