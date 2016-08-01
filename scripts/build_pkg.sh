#!/bin/bash

#
# Build and package fwup
#
# Inputs:
#     SKIP_PACKAGE - set to "true" to skip the package making step
#
# This script creates a static build of fwup to avoid dependency issues
# with libconfuse and libsodium on various Linux distributions. The result
# is a self-contained .deb and .rpm that should work on any Linux (assuming
# it's running on the same processor architecture)
#
set -e

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
BUILD_DIR=$BASE_DIR/build
DEPS_DIR=$BASE_DIR/deps
DEPS_INSTALL_DIR=$DEPS_DIR/usr
PKG_CONFIG_PATH=$DEPS_INSTALL_DIR/lib/pkgconfig

FWUP_INSTALL_DIR=$BUILD_DIR/fwup-installed/usr

MAKE_FLAGS=-j8

if [[ $(uname -s) = "Darwin" ]]; then
    EXTRA_LDFLAGS="-L/usr/local/lib -lintl"
fi

# Initial sanity checks
if [[ ! -e $BASE_DIR/configure ]]; then
    echo "Please run from the fwup base directory and make sure that the ./configure file exists."
    exit 1
fi

if [[ -e $BASE_DIR/Makefile ]]; then
    # Run distclean to ensure that we have a clean build.
    make distclean
fi

# Build the dependencies
$BASE_DIR/scripts/download_deps.sh

# Initialize some directories
mkdir -p $BUILD_DIR
mkdir -p $FWUP_INSTALL_DIR

pushd $BUILD_DIR

# Build fwup (symlink now, since out-of-tree fwup build is broke)
ln -sf $BASE_DIR $BUILD_DIR/fwup
pushd fwup
PKG_CONFIG_PATH=$PKG_CONFIG_PATH LDFLAGS="-L$DEPS_INSTALL_DIR/lib $EXTRA_LDFLAGS" CPPFLAGS=-I$DEPS_INSTALL_DIR/include ./configure --prefix=$FWUP_INSTALL_DIR --enable-shared=no || (cat config.log; exit 1)
make clean
make $MAKE_FLAGS

# Verify that it was statically linked
if ldd src/fwup | grep libz; then
    echo "fwup was dynamically linked to zlib. This should not happen.";
    exit 1
fi
if ldd src/fwup | grep confuse; then
    echo "fwup was dynamically linked to libconfuse. This should not happen.";
    exit 1
fi
if ldd src/fwup | grep archive; then
    echo "fwup was dynamically linked to libarchive. This should not happen.";
    exit 1
fi
if ldd src/fwup | grep sodium; then
    echo "fwup was dynamically linked to libsodium This should not happen.";
    exit 1
fi

# Run the regression tests
make check

make install-strip
make dist
popd

# Return to the base directory
popd

# Package fwup
if [[ "$SKIP_PACKAGE" != "true" ]]; then
    FWUP_VERSION=$($FWUP_INSTALL_DIR/bin/fwup --version)
    rm -f fwup_*.deb fwup-*.rpm
    fpm -s dir -t deb -v $FWUP_VERSION -n fwup -C $FWUP_INSTALL_DIR/..
    fpm -s dir -t rpm -v $FWUP_VERSION -n fwup -C $FWUP_INSTALL_DIR/..
else
    echo "Static build was successful, but skipped creating packages."
    echo "The fwup installation is in $FWUP_INSTALL_DIR."
fi

