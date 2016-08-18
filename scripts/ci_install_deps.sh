#!/bin/bash

#
# Install dependencies on Travis
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

LIBARCHIVE_VERSION=3.2.1
LIBSODIUM_VERSION=1.0.10
CONFUSE_VERSION=3.0

MAKE_FLAGS=-j4
if [[ "$TRAVIS_OS_NAME" = "linux" ]]; then
    DEPS_INSTALL_DIR=/usr
else
    DEPS_INSTALL_DIR=/usr/local
fi

install_confuse() {
    wget https://github.com/martinh/libconfuse/releases/download/v$CONFUSE_VERSION/confuse-$CONFUSE_VERSION.tar.xz
    tar xf confuse-$CONFUSE_VERSION.tar.xz
    pushd confuse-$CONFUSE_VERSION
    ./configure --prefix=$DEPS_INSTALL_DIR --disable-examples
    make $MAKE_FLAGS
    sudo make install
    popd
}

install_sodium() {
    wget https://download.libsodium.org/libsodium/releases/libsodium-$LIBSODIUM_VERSION.tar.gz
    tar xf libsodium-$LIBSODIUM_VERSION.tar.gz
    pushd libsodium-$LIBSODIUM_VERSION
    ./configure --prefix=$DEPS_INSTALL_DIR
    make $MAKE_FLAGS
    sudo make install
    popd
}

if [[ "$TRAVIS_OS_NAME" = "linux" ]]; then
    sudo apt-get update -qq
    sudo apt-get install -qq autopoint mtools unzip
    case $MODE in
        windows)
            sudo dpkg --add-architecture i386
            sudo apt-get update
            sudo apt-get install -y gcc-mingw-w64-x86-64 wine
            ;;
        dynamic)
            sudo apt-get install -qq libarchive-dev
            install_confuse
            install_sodium
            pip install --user cpp-coveralls
            ;;
        static)
            # Need fpm when building static so that we can make the .deb and .rpm packages
            sudo apt-get install -qq rpm
            gem install fpm
            ;;
    esac
else
    brew update
    brew install coreutils mtools gnu-sed
    brew install --universal gettext
    brew link --force gettext
    if [[ "$MODE" = "dynamic" ]]; then
        brew install libarchive libsodium confuse
    fi
    # Fix brew breakage in autotools
    mkdir /usr/local/Library/ENV
    ln -s /usr/local/Library/Homebrew/shims/super /usr/local/Library/ENV/4.3
    ls /usr/local/Library/ENV/4.3
fi

