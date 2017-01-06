#!/bin/sh

set -e

KERNEL=$1
FLASH_IMAGE=$2
HOST_SSH_PORT=$3

[ -n "$KERNEL" ] || KERNEL=images/u-boot
[ -n "$FLASH_IMAGE" ] || FLASH_IMAGE=images/vexpress.img
[ -n "$HOST_SSH_PORT" ] || HOST_SSH_PORT=10022

if [ -f "host/usr/bin/qemu-system-arm" ]
    QEMU=host/usr/bin/qemu-system-arm
else
    QEMU=qemu-system-arm
fi

echo "Invoking $QEMU with the following settings:"
echo
echo "Bootloader image: $KERNEL"
echo "Flash image: $FLASH_IMAGE"
echo
echo "Connect to this image by running:"
echo "ssh -p $HOST_SSH_PORT localhost"
echo

$QEMU -M vexpress-a9 -m 1024M \
    -kernel $KERNEL -drive file=$FLASH_IMAGE,if=sd,format=raw \
    -net nic,model=lan9118 -net user,hostfwd=tcp::$HOST_SSH_PORT-:22 -nographic

