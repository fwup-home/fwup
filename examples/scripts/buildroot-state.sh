#!/bin/bash

#
# Print information about the state of the Buildroot tree.
#
# Inputs:
#   $1   The Buildroot version
#   $2   The directory containing patches for Buildroot
#
# Outputs:
#   Text describing the Buildroot tree
#

set -e
export LC_ALL=C

BR_VERSION=$1
BR_PATCH_DIR=$2

usage() {
    echo "buildroot-state.sh <BR version> <patch directory>"
}

if [[ -z $BR_VERSION || -z $BR_PATCH_DIR ]]; then
    usage
    exit 1
fi
if [[ ! -d $BR_PATCH_DIR ]]; then
    echo "ERROR: Buildroot patch directory invalid"
    exit 1
fi

# Trust the passed in version
echo "Buildroot: $BR_VERSION"

pushd $BR_PATCH_DIR >/dev/null

# Run a SHA256 on all of the patches (in sorted order)
find . -name "*.patch" | sort | xargs sha256sum

popd >/dev/null
