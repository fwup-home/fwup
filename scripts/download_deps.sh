#!/bin/bash

#
# Download and build dependencies as static libs
#
set -e

ZLIB_VERSION=1.2.8
LIBARCHIVE_VERSION=3.2.1
LIBSODIUM_VERSION=1.0.10
CONFUSE_VERSION=3.0

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
DEPS_DIR=$BASE_DIR/deps
DOWNLOAD_DIR=$DEPS_DIR/dl
DEPS_INSTALL_DIR_NIX=$DEPS_DIR/nix/usr
DEPS_INSTALL_DIR_WIN=$DEPS_DIR/win/usr

MAKE_FLAGS=-j8

CONFIGURE_ARGS_WIN=--host=x86_64-w64-mingw32

# Initialize some directories
mkdir -p $DOWNLOAD_DIR
mkdir -p $DEPS_INSTALL_DIR_NIX
mkdir -p $DEPS_INSTALL_DIR_WIN

pushd $DEPS_DIR

pushd $DOWNLOAD_DIR
[[ -e zlib-$ZLIB_VERSION.tar.xz ]] || wget http://zlib.net/zlib-$ZLIB_VERSION.tar.xz
[[ -e confuse-$CONFUSE_VERSION.tar.xz ]] || wget https://github.com/martinh/libconfuse/releases/download/v$CONFUSE_VERSION/confuse-$CONFUSE_VERSION.tar.xz
[[ -e libarchive-$LIBARCHIVE_VERSION.tar.gz ]] || wget http://libarchive.org/downloads/libarchive-$LIBARCHIVE_VERSION.tar.gz
[[ -e libsodium-$LIBSODIUM_VERSION.tar.gz ]] || wget https://download.libsodium.org/libsodium/releases/libsodium-$LIBSODIUM_VERSION.tar.gz
popd

# Build zlib for Linux and Windows
if [[ ! -e $DEPS_INSTALL_DIR_NIX/lib/libz.a ]]; then
    rm -fr $DEPS_DIR/zlib-*
    tar xf $DOWNLOAD_DIR/zlib-$ZLIB_VERSION.tar.xz
    pushd zlib-$ZLIB_VERSION
    ./configure --prefix=$DEPS_INSTALL_DIR_NIX --static
    make $MAKE_FLAGS
    make install
    popd
fi

if [[ ! -e $DEPS_INSTALL_DIR_WIN/lib/libz.a ]]; then
    rm -fr $DEPS_DIR/zlib-*
    tar xf $DOWNLOAD_DIR/zlib-$ZLIB_VERSION.tar.xz
    pushd zlib-$ZLIB_VERSION
    CC=x86_64-w64-mingw32-gcc ./configure --prefix=$DEPS_INSTALL_DIR_WIN --static
    make $MAKE_FLAGS
    make install
    popd
fi

# Build libconfuse for Linux and Windows
if [[ ! -e $DEPS_INSTALL_DIR_NIX/lib/libconfuse.a ]]; then
    rm -fr $DEPS_DIR/confuse-*
    tar xf $DOWNLOAD_DIR/confuse-$CONFUSE_VERSION.tar.xz
    pushd confuse-$CONFUSE_VERSION
    ./configure --prefix=$DEPS_INSTALL_DIR_NIX --disable-examples --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

if [[ ! -e $DEPS_INSTALL_DIR_WIN/lib/libconfuse.a ]]; then
    rm -fr $DEPS_DIR/confuse-*
    tar xf $DOWNLOAD_DIR/confuse-$CONFUSE_VERSION.tar.xz
    pushd confuse-$CONFUSE_VERSION
    ./configure $CONFIGURE_ARGS_WIN --prefix=$DEPS_INSTALL_DIR_WIN --disable-examples --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

# Build libarchive for Linux and Windows
if [[ ! -e $DEPS_INSTALL_DIR_NIX/lib/libarchive.a ]]; then
    rm -fr libarchive-*
    tar xf $DOWNLOAD_DIR/libarchive-$LIBARCHIVE_VERSION.tar.gz
    pushd libarchive-$LIBARCHIVE_VERSION
    ./configure --prefix=$DEPS_INSTALL_DIR_NIX --without-xml2 --without-openssl --without-nettle --without-expat --without-lzo2 --without-lzma --without-bz2lib --without-iconv --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

if [[ ! -e $DEPS_INSTALL_DIR_WIN/lib/libarchive.a ]]; then
    rm -fr libarchive-*
    tar xf $DOWNLOAD_DIR/libarchive-$LIBARCHIVE_VERSION.tar.gz
    pushd libarchive-$LIBARCHIVE_VERSION
    ./configure $CONFIGURE_ARGS_WIN --prefix=$DEPS_INSTALL_DIR_WIN --without-xml2 --without-openssl --without-nettle --without-expat --without-lzo2 --without-lzma --without-bz2lib --without-iconv --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

# Build libsodium for Linux and Windows
if [[ ! -e $DEPS_INSTALL_DIR_NIX/lib/libsodium.a ]]; then
    rm -fr libsodium-*
    tar xf $DOWNLOAD_DIR/libsodium-$LIBSODIUM_VERSION.tar.gz
    pushd libsodium-$LIBSODIUM_VERSION
    ./configure --prefix=$DEPS_INSTALL_DIR_NIX --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

if [[ ! -e $DEPS_INSTALL_DIR_WIN/lib/libsodium.a ]]; then
    rm -fr libsodium-*
    tar xf $DOWNLOAD_DIR/libsodium-$LIBSODIUM_VERSION.tar.gz
    pushd libsodium-$LIBSODIUM_VERSION
    ./configure $CONFIGURE_ARGS_WIN --prefix=$DEPS_INSTALL_DIR_WIN --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

# Return to the base directory
popd

echo Dependencies built successfully!
echo
echo To compile fwup statically with these libraries, run:
echo
echo "./autogen.sh # if you're compiling from source"
echo "# For Linux:"
echo LDFLAGS=-L$DEPS_INSTALL_DIR_NIX/lib CPPFLAGS=-I$DEPS_INSTALL_DIR_NIX/include ./configure --enable-shared=no
echo "# For Windows"
echo LDFLAGS=-L$DEPS_INSTALL_DIR_WIN/lib CPPFLAGS=-I$DEPS_INSTALL_DIR_WIN/include ./configure $CONFIGURE_ARGS_WIN --enable-shared=no
echo make
echo make check
echo make install
