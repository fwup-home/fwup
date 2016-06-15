#!/bin/sh

set -e

# Run this script in a Cygwin terminal

mkdir -p win32
cd win32
BUILD_DIR=$(pwd)

# Initialize some directories
mkdir -p usr

if [ ! -e .downloaded ]; then
    wget https://github.com/martinh/libconfuse/releases/download/v2.8/confuse-2.8.tar.xz
    wget http://libarchive.org/downloads/libarchive-3.1.2.tar.gz
    wget https://download.libsodium.org/libsodium/releases/libsodium-1.0.8.tar.gz
    touch .downloaded
fi

if [ ! -e .confuse ]; then
    rm -fr confuse-2.8
    tar xf confuse-2.8.tar.xz
    cd confuse-2.8
    ./configure --host=x86_64-w64-mingw32 --prefix=$BUILD_DIR/usr --disable-examples
    make -j8
    make install
    cd ..
    touch .confuse
fi

if [ ! -e .libarchive ]; then
    rm -fr libarchive-3.1.2
    tar xf libarchive-3.1.2.tar.gz
    cd libarchive-3.1.2
    ./configure --host=x86_64-w64-mingw32 --prefix=$BUILD_DIR/usr
    make -j8
    make install
    cd ..
    touch .libarchive
fi

if [ ! -e .libsodium ]; then
    rm -fr libsodium-3.1.2
    tar xf libsodium-1.0.8.tar.gz
    cd libsodium-1.0.8
    ./configure --host=x86_64-w64-mingw32 --prefix=$BUILD_DIR/usr
    make -j8
    make install
    cd ..
    touch .libsodium
fi

mkdir -p fwup
cd fwup
LDFLAGS=-L$BUILD_DIR/usr/lib CPPFLAGS=-I$BUILD_DIR/usr/include ../../configure --host=x86_64-w64-mingw32 --prefix=$BUILD_DIR/usr
