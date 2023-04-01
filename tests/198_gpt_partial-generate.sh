#!/bin/sh

set -e

# Adapted from Buildroot

# GPT partition type UUIDs
esp_type=c12a7328-f81f-11d2-ba4b-00a0c93ec93b
linux_type=44479540-f297-41b2-9af7-d131d5f0458a

# Hardcode UUIDs for reproducible images (call uuidgen to make more)
disk_uuid=b443fbeb-2c93-481b-88b3-0ecb0aeba911
efi_part_uuid=5278721d-0089-4768-85df-b8f1b97e6684
root_part_uuid=fcc205c8-2f1c-4dcd-bef4-7b209aa15cca

# Boot partition offset and size, in 512-byte sectors
efi_part_start=64
efi_part_size=32768

# Rootfs partition offset and size, in 512-byte sectors
root_part_start=$(( efi_part_start + efi_part_size ))
root_part_size=65504

gpt_size=33

first_lba=$(( 1 + gpt_size ))
last_lba=$(( root_part_start + root_part_size ))

# Disk image size in 512-byte sectors
image_size=$(( last_lba + gpt_size + 1 ))

primary_gpt_lba=0
secondary_gpt_lba=$(( image_size - gpt_size ))

rm -f disk.img disk-primary-gpt.img disk-secondary-gpt.img
dd if=/dev/zero of=disk.img bs=512 count=0 seek=$image_size 2>/dev/null

sfdisk disk.img <<EOF
label: gpt
label-id: $disk_uuid
device: /dev/foobar0
unit: sectors
first-lba: $first_lba
last-lba: $last_lba

/dev/foobar0p1 : start=$efi_part_start,  size=$efi_part_size,  type=$esp_type,   uuid=$efi_part_uuid,  name="efi-part.vfat"
/dev/foobar0p2 : start=$root_part_start, size=$root_part_size, type=$linux_type, uuid=$root_part_uuid, name="rootfs.ext2"
EOF

echo
echo "Add the following to the unit test"
echo
echo "base64_decodez >\$WORK/expected-primary-gpt.img <<EOF"
dd if=disk.img bs=512 count=$gpt_size skip=$primary_gpt_lba 2>/dev/null | gzip -c | base64
echo "EOF"
echo "base64_decodez >\$WORK/expected-secondary-gpt.img <<EOF"
dd if=disk.img bs=512 count=$gpt_size skip=$secondary_gpt_lba 2>/dev/null | gzip -c | base64
echo "EOF"
echo "cp \$WORK/expected-primary-gpt.img \$WORK/expected.img"
echo "dd if=\$WORK/expected-secondary-gpt.img of=\$WORK/expected.img bs=512 seek=$secondary_gpt_lba conv=notrunc"


