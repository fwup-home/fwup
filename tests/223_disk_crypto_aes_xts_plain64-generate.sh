#!/bin/sh

set -e

algorithm="aes-xts-plain64"
# 512-bit key (128 hex chars) for AES-256-XTS
key_hex="8e9c0780fd7f5d00c18a30812fe960cfce71f6074dd9cded6aab2897568cc856fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"
partition_size=255
image_size=256
write_offset=1

rm -f partition.image disk.key disk.image
printf '%s' "$key_hex" | xxd -r -p > disk.key
dd if=/dev/zero of=partition.image bs=512 count=0 seek=$partition_size 2>/dev/null

loopdev=$(sudo losetup -f --show partition.image)
sudo cryptsetup open --type=plain --cipher=$algorithm --key-size 512 --key-file=disk.key "$loopdev" my_crypt
sudo dd if=1K.bin of=/dev/mapper/my_crypt
sudo cryptsetup close my_crypt
sudo losetup -d "$loopdev"

dd if=partition.image of=disk.image bs=512 seek=$write_offset 2>/dev/null

echo
echo "Add the following to the unit test"
echo
echo "base64_decodez >\$WORK/expected.img <<EOF"
dd if=disk.image bs=512 count=$image_size 2>/dev/null | gzip -c | base64
echo "EOF"

rm -f partition.image disk.key disk.image
