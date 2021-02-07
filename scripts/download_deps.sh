#!/bin/bash

#
# Download dependencies
#

set -e

BASE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

source $BASE_DIR/scripts/common.sh

if [ $(uname -s) = "Darwin" ]; then
    SHA256SUM="$(brew --prefix)/bin/gsha256sum"

    if [ ! -f $SHA256SUM ]; then
        echo "Please run 'brew install coreutils' for gsha256sum"
        exit 1
    fi
else
    SHA256SUM=sha256sum
fi

# Initialize some directories
mkdir -p $DOWNLOAD_DIR

echo "Downloading third party libraries to $DOWNLOAD_DIR..."

cd $DOWNLOAD_DIR
[ -e zlib-$ZLIB_VERSION.tar.xz ] || curl -LO http://zlib.net/zlib-$ZLIB_VERSION.tar.xz
[ -e confuse-$CONFUSE_VERSION.tar.xz ] || curl -LO https://github.com/martinh/libconfuse/releases/download/v$CONFUSE_VERSION/confuse-$CONFUSE_VERSION.tar.xz
[ -e libarchive-$LIBARCHIVE_VERSION.tar.gz ] || curl -LO http://libarchive.org/downloads/libarchive-$LIBARCHIVE_VERSION.tar.gz

echo "Verifying checksums..."
$SHA256SUM -c $BASE_DIR/scripts/third_party.sha256

