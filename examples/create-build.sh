#!/bin/bash

#
# Create and initialize a directory for building a fwup example.
#
# Inputs:
#   $1 = the path to the configuration file (a _defconfig file)
#   $2 = the build directory
#
# Output:
#   An initialized build directory on success
#

set -e

BUILDROOT_VERSION=2016.11.1

DEFCONFIG=$1
BUILD_DIR=$2

# "readlink -f" implementation for BSD
# This code was extracted from the Elixir shell scripts
readlink_f () {
    cd "$(dirname "$1")" > /dev/null
    filename="$(basename "$1")"
    if [[ -h "$filename" ]]; then
        readlink_f "$(readlink "$filename")"
    else
        echo "`pwd -P`/$filename"
    fi
}

if [[ -z $DEFCONFIG ]]; then
    echo "Usage:"
    echo
    echo "  $0 <defconfig> [build directory]"
    exit 1
fi

if [[ -z $BUILD_DIR ]]; then
    BUILD_DIR=o/$(basename -s _defconfig $DEFCONFIG)
fi

# Create the build directory if it doesn't already exist
mkdir -p $BUILD_DIR

# Normalize paths that were specified
ABS_DEFCONFIG=$(readlink_f $DEFCONFIG)
ABS_DEFCONFIG_DIR=$(dirname $ABS_DEFCONFIG)
ABS_BUILD_DIR=$(readlink_f $BUILD_DIR)

if [[ ! -f $ABS_DEFCONFIG ]]; then
    echo "ERROR: Can't find $ABS_DEFCONFIG. Please check that it exists."
    exit 1
fi

# Check that the host can build an image
HOST_OS=$(uname -s)
if [[ $HOST_OS != "Linux" ]]; then
    echo "ERROR: This only works on Linux"
    exit 1
fi

# Determine the BASE_DIR source directory
BASE_DIR=$(dirname $(readlink_f "${BASH_SOURCE[0]}"))
if [[ ! -e $BASE_DIR ]]; then
    echo "ERROR: Can't determine script directory!"
    exit 1
fi

# Location to download files to so that they don't need
# to be redownloaded when working a lot with buildroot
#
# NOTE: If you are a heavy Buildroot user and have an alternative location,
#       override this environment variable or symlink this directory.
if [[ -z $BUILDROOT_DL_DIR ]]; then
    if [[ -e $HOME/dl ]]; then
        BUILDROOT_DL_DIR=$HOME/dl
    fi
fi

BUILDROOT_STATE_FILE=$BASE_DIR/buildroot-$BUILDROOT_VERSION/.fwup-examples-br-state
BUILDROOT_EXPECTED_STATE_FILE=$BUILD_DIR/.fwup-examples-expected-br-state
$BASE_DIR/scripts/buildroot-state.sh $BUILDROOT_VERSION $BASE_DIR/patches/buildroot > $BUILDROOT_EXPECTED_STATE_FILE

create_buildroot_dir() {
    # Clean up any old versions of Buildroot
    rm -fr $BASE_DIR/buildroot*

    # Download and extract Buildroot
    $BASE_DIR/scripts/download-buildroot.sh $BUILDROOT_VERSION $BUILDROOT_DL_DIR $BASE_DIR

    # Apply patches
    $BASE_DIR/buildroot/support/scripts/apply-patches.sh $BASE_DIR/buildroot $BASE_DIR/patches/buildroot

    if ! [[ -z $BUILDROOT_DL_DIR ]]; then
        # Symlink Buildroot's dl directory so that it can be cached between builds
        ln -sf $BUILDROOT_DL_DIR $BASE_DIR/buildroot/dl
    fi

    cp $BUILDROOT_EXPECTED_STATE_FILE $BUILDROOT_STATE_FILE
}

if [[ ! -e $BUILDROOT_STATE_FILE ]]; then
    create_buildroot_dir
elif ! diff $BUILDROOT_STATE_FILE $BUILDROOT_EXPECTED_STATE_FILE >/dev/null; then
    echo "Detected a difference in the Buildroot source tree either due"
    echo "to an change in Buildroot or a change in the patches that get"
    echo "applied to Buildroot. The Buildroot source tree will be updated."
    echo
    echo "It is highly recommended to rebuild clean."
    echo "To do this, go to $BUILD_DIR, and run 'make clean'."
    echo
    echo "Press return to acknowledge or CTRL-C to stop"
    read
    create_buildroot_dir
fi

# Configure the build directory - finally!
make -C $BASE_DIR/buildroot BR2_EXTERNAL=$BASE_DIR O=$ABS_BUILD_DIR \
    BR2_DEFCONFIG=$ABS_DEFCONFIG \
    DEFCONFIG=$ABS_DEFCONFIG \
    defconfig

echo "------------"
echo
echo "Build directory successfully created."
echo
echo "Configuration: $ABS_DEFCONFIG"
echo
echo "Next, do the following:"
echo "   1. cd $ABS_BUILD_DIR"
echo "   2. make"
echo
echo "For additional options, run 'make help' in the build directory."
echo
echo "IMPORTANT: If you update fwup-examples, you should rerun this script."
echo "           It will refresh the configuration in the build directory."

