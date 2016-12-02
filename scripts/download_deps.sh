#!/bin/bash

#
# Download dependencies
#

set -e

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

source $BASE_DIR/scripts/common.sh

# Initialize some directories
mkdir -p $DOWNLOAD_DIR

cd $DOWNLOAD_DIR
[ -e zlib-$ZLIB_VERSION.tar.xz ] || curl -LO http://zlib.net/zlib-$ZLIB_VERSION.tar.xz
[ -e confuse-$CONFUSE_VERSION.tar.xz ] || curl -LO https://github.com/martinh/libconfuse/releases/download/v$CONFUSE_VERSION/confuse-$CONFUSE_VERSION.tar.xz
[ -e libarchive-$LIBARCHIVE_VERSION.tar.gz ] || curl -LO http://libarchive.org/downloads/libarchive-$LIBARCHIVE_VERSION.tar.gz
[ -e libsodium-$LIBSODIUM_VERSION.tar.gz ] || curl -LO https://github.com/jedisct1/libsodium/releases/download/$LIBSODIUM_VERSION/libsodium-$LIBSODIUM_VERSION.tar.gz

