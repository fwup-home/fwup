#!/bin/sh

set -e

# Adapted from Buildroot

# GPT partition type UUIDs
esp_type=c12a7328-f81f-11d2-ba4b-00a0c93ec93b
linux_root_arm_type=69DAD710-2CE4-4E3C-B16C-21A1D49ABED3
linux_type=0FC63DAF-8483-4772-8E79-3D69D8477DE4

# Hardcode UUIDs for reproducible images (call uuidgen to make more)
disk_uuid=b443fbeb-2c93-481b-88b3-0ecb0aeba911
efi_part_uuid=5278721d-0089-4768-85df-b8f1b97e6684
rootfs_a_part_uuid=fcc205c8-2f1c-4dcd-bef4-7b209aa15cca
rootfs_b_part_uuid=4E69D2CD-028F-40CE-BBEE-94FB353B424C
app_part_uuid=9558571B-1DFC-4C3F-8DB4-A8A564FB99A4

gpt_size=33

# Boot partition offset and size, in 512-byte sectors
efi_part_start=1024
efi_part_size=1024
rootfs_a_part_start=2048
rootfs_a_part_size=2048
rootfs_b_part_start=4096
rootfs_b_part_size=2048
app_part_start=8192
app_part_size=4096

# Force the image size to be a multiple of 128 KB. fwup will round
# to the nearest multiple since it only works in 128 KB chunks.
image_expanded_size=20736
app_part_expanded_size=$(( image_expanded_size - gpt_size - 1 - app_part_start ))

first_lba=$(( 1 + gpt_size ))
last_lba=$(( app_part_start + app_part_size ))
last_lba_expanded=$(( app_part_start + app_part_expanded_size ))

# Disk image size in 512-byte sectors
image_size=$(( last_lba + gpt_size + 1 ))

primary_gpt_lba=0
secondary_gpt_lba=$(( image_size - gpt_size ))
secondary_gpt_lba_expanded=$(( image_expanded_size - gpt_size ))

rm -f disk.img disk-primary-gpt.img disk-secondary-gpt.img disk_expanded.img
dd if=/dev/zero of=disk.img bs=512 count=0 seek=$image_size 2>/dev/null
dd if=/dev/zero of=disk_expanded.img bs=512 count=0 seek=$image_expanded_size 2>/dev/null

sfdisk disk.img <<EOF
label: gpt
label-id: $disk_uuid
device: /dev/foobar0
unit: sectors
first-lba: $first_lba
last-lba: $last_lba

/dev/foobar0p1 : start=$efi_part_start,  size=$efi_part_size,  type=$esp_type,   uuid=$efi_part_uuid,  name="efi-part.vfat"
/dev/foobar0p2 : start=$rootfs_a_part_start, size=$rootfs_a_part_size, type=$linux_root_arm_type, uuid=$rootfs_a_part_uuid, name="rootfs.a.ext2"
/dev/foobar0p3 : start=$rootfs_b_part_start, size=$rootfs_b_part_size, type=$linux_root_arm_type, uuid=$rootfs_b_part_uuid, name="rootfs.b.ext2"
/dev/foobar0p4 : start=$app_part_start, size=$app_part_size, type=$linux_type, uuid=$app_part_uuid, name="app"
EOF

sfdisk disk_expanded.img <<EOF
label: gpt
label-id: $disk_uuid
device: /dev/foobar0
unit: sectors
first-lba: $first_lba
last-lba: $last_lba_expanded

/dev/foobar0p1 : start=$efi_part_start,  size=$efi_part_size,  type=$esp_type,   uuid=$efi_part_uuid,  name="efi-part.vfat"
/dev/foobar0p2 : start=$rootfs_a_part_start, size=$rootfs_a_part_size, type=$linux_root_arm_type, uuid=$rootfs_a_part_uuid, name="rootfs.a.ext2"
/dev/foobar0p3 : start=$rootfs_b_part_start, size=$rootfs_b_part_size, type=$linux_root_arm_type, uuid=$rootfs_b_part_uuid, name="rootfs.b.ext2"
/dev/foobar0p4 : start=$app_part_start, size=$app_part_expanded_size, type=$linux_type, uuid=$app_part_uuid, name="app"
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
echo "cp \$WORK/expected-primary-gpt.img \$WORK/unexpanded.img"
echo "dd if=\$WORK/expected-secondary-gpt.img of=\$WORK/unexpanded.img bs=512 seek=$secondary_gpt_lba conv=notrunc"
echo
echo "base64_decodez >\$WORK/expected-primary-gpt2.img <<EOF"
dd if=disk_expanded.img bs=512 count=$gpt_size skip=$primary_gpt_lba 2>/dev/null | gzip -c | base64
echo "EOF"
echo "base64_decodez >\$WORK/expected-secondary-gpt2.img <<EOF"
dd if=disk_expanded.img bs=512 count=$gpt_size skip=$secondary_gpt_lba_expanded 2>/dev/null | gzip -c | base64
echo "EOF"
echo "cp \$WORK/expected-primary-gpt2.img \$WORK/expanded.img"
echo "dd if=\$WORK/expected-secondary-gpt2.img of=\$WORK/expanded.img bs=512 seek=$secondary_gpt_lba_expanded conv=notrunc"

