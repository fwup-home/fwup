#!/usr/bin/env bash

set -e

TARGETDIR=$1
FWUP_CONFIG=$2
FWNAME=$3

if [[ -z $FWUP_CONFIG ]]; then
    echo "Please specify a path to a fwup config file"
    exit 1
fi

if [[ -z $FWNAME ]]; then
    FWNAME=vexpress
fi

FWUP=$HOST_DIR/usr/bin/fwup

FW_PATH=$BINARIES_DIR/$FWNAME.fw
IMG_PATH=$BINARIES_DIR/$FWNAME.img

# Strip u-boot since it gets copied w/ symbols by default
# and the symbols can get really large
$HOST_DIR/usr/bin/arm-linux-gnueabihf-strip $BINARIES_DIR/u-boot

# Build the firmware image (.fw file)
echo "Creating firmware file..."
PROJECT_ROOT=$BR2_EXTERNAL_FWUP_EXAMPLES_PATH $FWUP -c -f $FWUP_CONFIG -o $FW_PATH

# Build a raw image that can be used by qemu-system
echo "Creating raw SDCard/eMMC image file..."
rm -f $IMG_PATH
$FWUP -a -d $IMG_PATH -i $FW_PATH -t complete

