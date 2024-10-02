#!/usr/bin/env bash

#
# Install dependencies on CircleCi
#
# Inputs:
#    CIRCLE_OS_NAME - "linux" or "osx"
#    BUILD_STATIC   - "true" or "false"
#
# Static builds use scripts to download libarchive and libconfuse
# so those are only installed on shared library builds.
#

set -e
set -v

source scripts/third_party_versions.sh

MAKE_FLAGS=-j4
if [[ "$CIRCLE_OS_NAME" = "linux" ]]; then
    DEPS_INSTALL_DIR=/usr
else
    DEPS_INSTALL_DIR=/usr/local
fi

install_confuse() {
    echo Downloading and installing libconfuse $CONFUSE_VERSION...
    curl -LO https://github.com/libconfuse/libconfuse/releases/download/v$CONFUSE_VERSION/confuse-$CONFUSE_VERSION.tar.gz
    tar xf confuse-$CONFUSE_VERSION.tar.gz
    pushd confuse-$CONFUSE_VERSION
    ./configure --prefix=$DEPS_INSTALL_DIR --disable-examples
    make $MAKE_FLAGS
    make install
    popd
}

if [[ "$CIRCLE_OS_NAME" = "linux" ]]; then
    apt-get update -qq
    apt-get install -qq git autopoint dosfstools mtools unzip zip help2man autoconf build-essential libtool curl pkg-config mtools unzip zip help2man ca-certificates xdelta3 u-boot-tools
    case $MODE in
        windows)
            dpkg --add-architecture i386
            apt-get update
            apt-get install -qq gcc-mingw-w64-x86-64 wine
            ;;
        singlethread|dynamic|minimal)
            apt-get install -qq libarchive-dev
            install_confuse
            ;;
        static)
            #apt-get install -qq rpm
            ;;
        raspberrypi)
            apt-get install -qq libarchive-dev qemu binfmt-support qemu-user-static
            pushd ~
            git clone https://github.com/raspberrypi/tools.git --depth 1
            popd
            ;;

    esac
else
    # OSX
    BREW_PACKAGES="pkg-config coreutils mtools dosfstools xdelta libtool automake"
    if [[ "$MODE" = "dynamic" ]]; then
        BREW_PACKAGES="$BREW_PACKAGES libarchive confuse"
    fi

    brew install $BREW_PACKAGES
fi

