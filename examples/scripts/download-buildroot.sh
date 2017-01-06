#!/bin/bash

#
# Download and extract a specified version of buildroot. If the buildroot
# release tarball has been cached, use that one. If not, clone and cache the results.
#
# download-buildroot.sh <hash tag or version> [cached download directory] [extraction location]
#

set -e
export LC_ALL=C

HASHTAG=$1
DL=$2
WHERE=$3

[[ -z "$HASHTAG" ]] && { echo "Need to specify a hash tag or version to get"; exit 1; }

if [[ -z "$DL" ]]; then
    DL=$(pwd)
    echo "Download directory unspecified. Using $DL"
fi

if [[ -z "$DL" ]]; then
    WHERE=$(pwd)
fi

EXTRACTED_DIRNAME=$WHERE/buildroot-$HASHTAG
TARBALL_NAME=buildroot-$HASHTAG.tar.gz
CACHED_TARBALL_PATH=$DL/$TARBALL_NAME
BUILDROOT_PATH=$WHERE/buildroot

# Clean up in case previous extraction failed.
rm -fr $EXTRACTED_DIRNAME $BUILDROOT_PATH

if [[ ! -e "$CACHED_TARBALL_PATH" ]]; then
    # Download from Buildroot
    if [[ $HASHTAG =~ 20[0-9][0-9]\.[0-1][0-9] ]]; then
        # This is an official release and is hosted on the main
        # download site.
        DOWNLOAD_URL=https://buildroot.org/downloads/$TARBALL_NAME
    else
        # This is an intermediate release and can be downloaded from
        # Buildroot's cgit instance.
        DOWNLOAD_URL=https://git.busybox.net/buildroot/snapshot/$TARBALL_NAME
    fi

    # If the above URLs fail, we host a backup.
    ALTERNATIVE_URL=https://s3.amazonaws.com/nerves/build/$TARBALL_NAME

    pushd $DL
    if ! wget $DOWNLOAD_URL; then
      echo "Failed to download $TARBALL_NAME from primary location $DOWNLOAD_URL."
      echo "Attempting to download from $ALTERNATIVE_URL instead..."
      if ! wget $ALTERNATIVE_URL; then
        echo "Failed to download buildroot $TARBALL_NAME from $ALTERNATIVE_URL.";
        exit 1;
      fi
    fi
    popd
fi

# Extract the cached tarball
# We can't rely on the first level directory naming, so force it to
# the expected path
mkdir -p $EXTRACTED_DIRNAME
tar xzf $CACHED_TARBALL_PATH -C $EXTRACTED_DIRNAME --strip-components=1

# Symlink for easier access
ln -s $EXTRACTED_DIRNAME $BUILDROOT_PATH

echo "Buildroot extracted to $BUILDROOT_PATH."
