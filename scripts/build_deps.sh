#!/bin/bash

#
# Download and build dependencies as static libs
#
# Inputs:
#     CROSS_COMPILE - if set to a gcc tuple, tries to crosscompile
#                     (e.g., x86_64-w64-mingw32)
#
set -e

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

source $BASE_DIR/scripts/common.sh

# Download dependencies if not done already
$BASE_DIR/scripts/download_deps.sh

# Initialize some directories
mkdir -p $DEPS_INSTALL_DIR
cd $DEPS_DIR

# Build zlib
if [[ ! -e $DEPS_INSTALL_DIR/lib/libz.a ]]; then
    rm -fr $DEPS_DIR/zlib-*
    tar xf $DOWNLOAD_DIR/zlib-$ZLIB_VERSION.tar.xz
    cd zlib-$ZLIB_VERSION
    if [ -n "$CROSS_COMPILE" ]; then
        ZLIB_CONFIGURE_ENV="CC=$CROSS_COMPILE-gcc"
    fi
    (export $ZLIB_CONFIGURE_ENV; PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure --prefix=$DEPS_INSTALL_DIR --static)
    make $MAKE_FLAGS
    make install
    cd $DEPS_DIR
fi

# Build libconfuse
if [[ ! -e $DEPS_INSTALL_DIR/lib/libconfuse.a ]]; then
    rm -fr $DEPS_DIR/confuse-*
    tar xf $DOWNLOAD_DIR/confuse-$CONFUSE_VERSION.tar.xz
    cd confuse-$CONFUSE_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --prefix=$DEPS_INSTALL_DIR --disable-examples --enable-shared=no
    make $MAKE_FLAGS
    make install
    cd $DEPS_DIR
fi

# Build libarchive
if [[ ! -e $DEPS_INSTALL_DIR/lib/libarchive.a ]]; then
    rm -fr libarchive-*
    tar xf $DOWNLOAD_DIR/libarchive-$LIBARCHIVE_VERSION.tar.gz
    cd libarchive-$LIBARCHIVE_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH LDFLAGS=-L$DEPS_INSTALL_DIR/lib CPPFLAGS=-I$DEPS_INSTALL_DIR/include ./configure \
        $CONFIGURE_ARGS \
        --prefix=$DEPS_INSTALL_DIR --without-xml2 --without-openssl \
        --without-nettle --without-expat --without-lzo2 --without-lzma \
        --without-bz2lib --without-iconv --enable-shared=no --disable-bsdtar \
        --disable-bsdcpio --disable-bsdcat --disable-xattr \
        --without-libiconv-prefix --without-lz4
    make $MAKE_FLAGS
    make install
    cd $DEPS_DIR
fi

# Build libsodium
if [[ ! -e $DEPS_INSTALL_DIR/lib/libsodium.a ]]; then
    rm -fr libsodium-*
    tar xf $DOWNLOAD_DIR/libsodium-$LIBSODIUM_VERSION.tar.gz
    cd libsodium-$LIBSODIUM_VERSION
    PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --prefix=$DEPS_INSTALL_DIR --enable-shared=no
    make $MAKE_FLAGS
    make install
    cd $DEPS_DIR
fi

# Return to the base directory
cd $BASE_DIR

echo Dependencies built successfully!
echo
echo To compile fwup statically with these libraries, run:
echo
echo "./autogen.sh # if you're compiling from source"
echo PKG_CONFIG_PATH=$PKG_CONFIG_PATH ./configure $CONFIGURE_ARGS --enable-shared=no
echo make
echo make check
echo make install
