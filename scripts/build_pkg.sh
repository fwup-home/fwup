#!/bin/bash

#
# Build and package fwup
#
# This script creates a static build of fwup to avoid dependency issues
# with libconfuse and libsodium on various Linux distributions. The result
# is a self-contained .deb and .rpm that should work on any Linux (assuming
# it's running on the same processor architecture)
#
set -e

BASE_DIR=$(pwd)
BUILD_DIR=$BASE_DIR/pkg_build
DOWNLOAD_DIR=$BASE_DIR/dl
INSTALL_DIR=$BUILD_DIR/usr

FWUP_INSTALL_DIR=$BUILD_DIR/fwup-installed/usr

MAKE_FLAGS=-j8

# Initial sanity checks
if [[ ! -e $BASE_DIR/configure ]]; then
    echo "Please run from the fwup base directory and make sure that the ./configure file exists."
    exit 1
fi

if [[ -e $BASE_DIR/Makefile ]]; then
    # Run distclean to ensure that we have a clean build.
    make distclean
fi

# Initialize some directories
mkdir -p $DOWNLOAD_DIR
mkdir -p $BUILD_DIR
mkdir -p $INSTALL_DIR
mkdir -p $FWUP_INSTALL_DIR

pushd $BUILD_DIR

if [[ ! -e $DOWNLOAD_DIR/.downloaded ]]; then
    pushd $DOWNLOAD_DIR
    wget https://github.com/martinh/libconfuse/releases/download/v2.8/confuse-2.8.tar.xz
    wget http://libarchive.org/downloads/libarchive-3.1.2.tar.gz
    wget https://download.libsodium.org/libsodium/releases/libsodium-1.0.8.tar.gz
    touch $DOWNLOAD_DIR/.downloaded
    popd
fi

if [[ ! -e $INSTALL_DIR/lib/libconfuse.a ]]; then
    rm -fr $BUILD_DIR/confuse-2.8
    tar xf $DOWNLOAD_DIR/confuse-2.8.tar.xz
    pushd confuse-2.8
    ./configure --prefix=$INSTALL_DIR --disable-examples --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

if [[ ! -e $INSTALL_DIR/lib/libarchive.a ]]; then
    rm -fr libarchive-3.1.2
    tar xf $DOWNLOAD_DIR/libarchive-3.1.2.tar.gz
    pushd libarchive-3.1.2
    ./configure --prefix=$INSTALL_DIR --without-xml2 --without-openssl --without-nettle --without-expat --without-lzo2 --without-lzma --without-bz2lib --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

if [[ ! -e $INSTALL_DIR/lib/libsodium.a ]]; then
    rm -fr libsodium-3.1.2
    tar xf $DOWNLOAD_DIR/libsodium-1.0.8.tar.gz
    pushd libsodium-1.0.8
    ./configure --prefix=$INSTALL_DIR --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

# Build fwup (symlink now, since out-of-tree fwup build is broke)
ln -sf $BASE_DIR $BUILD_DIR/fwup
pushd fwup
LDFLAGS=-L$INSTALL_DIR/lib CPPFLAGS=-I$INSTALL_DIR/include ./configure --prefix=$FWUP_INSTALL_DIR --enable-shared=no
make clean
make $MAKE_FLAGS
make check
make install-strip
make dist
popd

# Return to the base directory
popd

# Package fwup
FWUP_VERSION=$($FWUP_INSTALL_DIR/bin/fwup --version)
rm -f fwup_*.deb fwup-*.rpm
fpm -s dir -t deb -v $FWUP_VERSION -n fwup -C $FWUP_INSTALL_DIR/..
fpm -s dir -t rpm -v $FWUP_VERSION -n fwup -C $FWUP_INSTALL_DIR/..

