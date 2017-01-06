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
UBINIZE=$HOST_DIR/usr/sbin/ubinize

FW_PATH=$BINARIES_DIR/$FWNAME.fw
IMG_PATH=$BINARIES_DIR/$FWNAME.img

# Strip u-boot since it gets copied w/ symbols by default
# and the symbols can get really large
$HOST_DIR/usr/bin/arm-linux-gnueabihf-strip $BINARIES_DIR/u-boot

# Create the images for the Vexpress's two NOR Flashes
# This currently assumes that the UBI only needs to initialize 1 of the 2.
# Empty regions are filled with 0xff's.
$UBINIZE -o $BINARIES_DIR/norflash1.img  -m 0x1 -p 0x80000 squash-config.ini
dd if=/dev/zero bs=64M count=1 | tr '\000' $'\377' >> $BINARIES_DIR/norflash1.img
truncate -s 64M $BINARIES_DIR/norflash1.img
dd if=/dev/zero bs=64M count=1 | tr '\000' $'\377' > $BINARIES_DIR/norflash2.img

if false; then
	# It is critical that qemu-system-arm is patched to support a flash interleave
	# of 2 or only half of the data that Linux writes will end up in the flash.
	$HOST_DIR/usr/bin/qemu-system-arm -M vexpress-a9 -smp 1 -m 256 -kernel images/zImage \
	    -dtb images/vexpress-v2p-ca9.dtb \
	    -append "console=ttyAMA0,115200 ubi.mtd=0 ubi.block=0,rootfs_a root=/dev/ubiblock0_0 rootfstype=squashfs" \
	    -drive file=$BINARIES_DIR/norflash1.img,if=pflash,index=0,format=raw \
	    -drive file=$BINARIES_DIR/norflash2.img,if=pflash,index=1,format=raw \
	    -nographic -net nic,model=lan9118 -net user,hostfwd=tcp::10022-:22
fi
