#!/bin/sh

#
# Shared logic for all scripts
#
# Inputs:
#     BASE_DIR - the root fwup directory
#     CROSS_COMPILE - if set to a gcc tuple, tries to crosscompile
#                     (e.g., x86_64-w64-mingw32)
#

FWUP_MAINTAINER="Frank Hunleth <fhunleth@troodon-software.com>"
FWUP_DESCRIPTION="Configurable embedded Linux firmware update creator and runner"
FWUP_HOMEPAGE="https://github.com/fwup-home/fwup"
FWUP_VENDOR="fhunleth@troodon-software.com"
FWUP_LICENSE="Apache-2.0"

source $BASE_DIR/scripts/third_party_versions.sh

MAKE_FLAGS=-j8
LDD=ldd

if [ -z "$CROSS_COMPILE" ]; then
    # Not cross-compiling
    BUILD_DIR=$BASE_DIR/build/host

    if [ $(uname -s) = "Darwin" ]; then
        LDD="otool -L"
    fi
else
    # Cross-compiling
    CONFIGURE_ARGS=--host=$CROSS_COMPILE
    BUILD_DIR=$BASE_DIR/build/$CROSS_COMPILE
fi

DOWNLOAD_DIR=$BASE_DIR/build/dl

DEPS_DIR=$BUILD_DIR/deps
DEPS_INSTALL_DIR=$DEPS_DIR/usr
FWUP_STAGING_DIR=$BUILD_DIR/fwup-staging/
FWUP_INSTALL_DIR=$FWUP_STAGING_DIR/usr
PKG_CONFIG_PATH=$DEPS_INSTALL_DIR/lib/pkgconfig

