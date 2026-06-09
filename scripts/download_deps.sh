#!/usr/bin/env bash

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

download() {
    if [ ! -e "$1" ]; then
        echo "Downloading $2..."
        curl -fL --retry 3 --retry-delay 2 --max-time 120 -o "$1" "$2"
    fi
}

cd $DOWNLOAD_DIR
download zlib-$ZLIB_VERSION.tar.gz https://zlib.net/fossils/zlib-$ZLIB_VERSION.tar.gz
download confuse-$CONFUSE_VERSION.tar.gz https://github.com/libconfuse/libconfuse/releases/download/v$CONFUSE_VERSION/confuse-$CONFUSE_VERSION.tar.gz
download libarchive-$LIBARCHIVE_VERSION.tar.gz https://libarchive.org/downloads/libarchive-$LIBARCHIVE_VERSION.tar.gz

echo "Verifying checksums..."
$SHA256SUM -c $BASE_DIR/scripts/third_party.sha256
