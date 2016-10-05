#!/bin/bash

#
# Download and build dependencies as static libs
#
# Inputs:
#     SKIP_PACKAGE  - set to "true" to skip the package making step
#     CROSS_COMPILE - if set to a gcc tuple, tries to crosscompile
#                     (e.g., x86_64-w64-mingw32)
#
set -e

ZLIB_VERSION=1.2.8
LIBARCHIVE_VERSION=3.2.1
LIBSODIUM_VERSION=1.0.11
CONFUSE_VERSION=3.0
CHOCO_VERSION=0.10.0

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
BUILD_DIR=$BASE_DIR/build
DOWNLOAD_DIR=$BUILD_DIR/dl

if [[ -z $CROSS_COMPILE ]]; then
    CROSS_COMPILE=host
else
    CONFIGURE_ARGS=--host=$CROSS_COMPILE
    ZLIB_CONFIGURE_ENV="CC=$CROSS_COMPILE-gcc"
fi

DEPS_DIR=$BUILD_DIR/$CROSS_COMPILE/deps
DEPS_INSTALL_DIR=$DEPS_DIR/usr
PKG_CONFIG_PATH=$DEPS_INSTALL_DIR/lib/pkgconfig

MAKE_FLAGS=-j8

# Initialize some directories
mkdir -p $DOWNLOAD_DIR
mkdir -p $DEPS_INSTALL_DIR

pushd $DEPS_DIR

pushd $DOWNLOAD_DIR
[[ -e zlib-$ZLIB_VERSION.tar.xz ]] || wget http://zlib.net/zlib-$ZLIB_VERSION.tar.xz
[[ -e confuse-$CONFUSE_VERSION.tar.xz ]] || wget https://github.com/martinh/libconfuse/releases/download/v$CONFUSE_VERSION/confuse-$CONFUSE_VERSION.tar.xz
[[ -e libarchive-$LIBARCHIVE_VERSION.tar.gz ]] || wget http://libarchive.org/downloads/libarchive-$LIBARCHIVE_VERSION.tar.gz
[[ -e libsodium-$LIBSODIUM_VERSION.tar.gz ]] || wget https://github.com/jedisct1/libsodium/releases/download/$LIBSODIUM_VERSION/libsodium-$LIBSODIUM_VERSION.tar.gz
if [[ "$CROSS_COMPILE" = "x86_64-w64-mingw32" ]]; then
    [[ -e choco-$CHOCO_VERSION.tar.gz ]] || wget -O choco-$CHOCO_VERSION.tar.gz https://github.com/chocolatey/choco/archive/$CHOCO_VERSION.tar.gz
fi
popd

# Build zlib
if [[ ! -e $DEPS_INSTALL_DIR/lib/libz.a ]]; then
    rm -fr $DEPS_DIR/zlib-*
    tar xf $DOWNLOAD_DIR/zlib-$ZLIB_VERSION.tar.xz
    pushd zlib-$ZLIB_VERSION
    (export $ZLIB_CONFIGURE_ENV; PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure --prefix=$DEPS_INSTALL_DIR --static)
    make $MAKE_FLAGS
    make install
    popd
fi

# Build libconfuse
if [[ ! -e $DEPS_INSTALL_DIR/lib/libconfuse.a ]]; then
    rm -fr $DEPS_DIR/confuse-*
    tar xf $DOWNLOAD_DIR/confuse-$CONFUSE_VERSION.tar.xz
    pushd confuse-$CONFUSE_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --prefix=$DEPS_INSTALL_DIR --disable-examples --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

# Build libarchive
if [[ ! -e $DEPS_INSTALL_DIR/lib/libarchive.a ]]; then
    rm -fr libarchive-*
    tar xf $DOWNLOAD_DIR/libarchive-$LIBARCHIVE_VERSION.tar.gz
    pushd libarchive-$LIBARCHIVE_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH LDFLAGS=-L$DEPS_INSTALL_DIR/lib CPPFLAGS=-I$DEPS_INSTALL_DIR/include ./configure \
        $CONFIGURE_ARGS \
        --prefix=$DEPS_INSTALL_DIR --without-xml2 --without-openssl \
        --without-nettle --without-expat --without-lzo2 --without-lzma \
        --without-bz2lib --without-iconv --enable-shared=no --disable-bsdtar \
        --disable-bsdcpio --disable-bsdcat --disable-xattr \
        --without-libiconv-prefix --without-lz4
    make $MAKE_FLAGS
    make install
    popd
fi

# Build libsodium
if [[ ! -e $DEPS_INSTALL_DIR/lib/libsodium.a ]]; then
    rm -fr libsodium-*
    tar xf $DOWNLOAD_DIR/libsodium-$LIBSODIUM_VERSION.tar.gz
    pushd libsodium-$LIBSODIUM_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --prefix=$DEPS_INSTALL_DIR --enable-shared=no
    make $MAKE_FLAGS
    make install
    popd
fi

# Chocolatey
if [[ "$CROSS_COMPILE" = "x86_64-w64-mingw32" ]] &&
    [[ ! -e $DEPS_INSTALL_DIR/chocolatey ]] &&
    [[ "$SKIP_PACKAGE" != "true" ]]; then
    rm -fr choco-*
    tar xf $DOWNLOAD_DIR/choco-$CHOCO_VERSION.tar.gz
    pushd choco-$CHOCO_VERSION
    nuget restore src/chocolatey.sln
    chmod +x build.sh
    ./build.sh -v
    cp -Rf code_drop/chocolatey $DEPS_INSTALL_DIR/
    popd
fi

# Return to the base directory
popd

echo Dependencies built successfully!
echo
echo To compile fwup statically with these libraries, run:
echo
echo "./autogen.sh # if you're compiling from source"
echo PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --enable-shared=no
echo make
echo make check
echo make install
