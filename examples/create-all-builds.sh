#!/bin/sh

set -e

configs=$(find ./configs -name "*_defconfig")

for config in $configs; do
    base=$(basename -s _defconfig $config)

    echo "Creating $config..."
    ./create-build.sh $config
done

