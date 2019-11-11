#!/bin/sh

set -e

algorithm="aes-cbc-plain"
key="jpwHgP1/XQDBijCBL+lgz85x9gdN2c3taqsol1aMyFY="
partition_size=255
image_size=256
write_offset=1

rm -f partition.image disk.key disk.image
echo $key | base64 -d > disk.key
dd if=/dev/zero of=partition.image bs=512 count=0 seek=$partition_size 2>/dev/null

sudo losetup -d /dev/loop7 2>/dev/null || true
sudo losetup /dev/loop7 partition.image
sudo cryptsetup open --type=plain --cipher=$algorithm --key-file=disk.key /dev/loop7 my_crypt
sudo dd if=1K.bin of=/dev/mapper/my_crypt
sudo cryptsetup close my_crypt
sudo losetup -d /dev/loop7 2>/dev/null || true

dd if=partition.image of=disk.image bs=512 seek=$write_offset 2>/dev/null

echo
echo "Add the following to the unit test"
echo
echo "base64_decodez >\$WORK/expected.img <<EOF"
dd if=disk.image bs=512 count=$image_size 2>/dev/null | gzip -c | base64
echo "EOF"

rm -f partition.image disk.key disk.image
