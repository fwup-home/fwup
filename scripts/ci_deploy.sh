#!/bin/bash

#
# Deploy a successful build on Travis
#
# Inputs:
#  CI                  - On CI, this is "true"
#  GITHUB_HEAD_REF     - Must not be defined since GitHub Actions should not invoke this script on pull requests
#  GITHUB_REF          - The tag name if a tagged build
#  MODE                - "static", "dynamic", "singlethread", or "windows"
#

set -e
set -v

if [[ "$CI" != "true" ]]; then
    echo "This script is intended to be run on Travis"
    exit 1
fi

if [[ -n "$GITHUB_HEAD_REF" ]]; then
    echo "Not supposed to be deploying pull requests!!"
    exit 1
fi

ARTIFACT_SUBDIR=$GITHUB_REF
OS_NAME="$(uname -s)"

# Copy the artifacts to a location that's easy to reference in the .travis.yml
rm -fr artifacts
mkdir -p artifacts/$ARTIFACT_SUBDIR

case "${OS_NAME}-${MODE}" in
    Linux-static)
        cp fwup-*.rpm artifacts/$ARTIFACT_SUBDIR/
        cp fwup_*.deb artifacts/$ARTIFACT_SUBDIR/
        cp fwup-*.tar.gz artifacts/$ARTIFACT_SUBDIR/
        ;;
    Linux-windows)
        cp fwup.exe artifacts/$ARTIFACT_SUBDIR/
        cp fwup.*.nupkg artifacts/$ARTIFACT_SUBDIR/
        ;;
    Linux-raspberrypi)
        cp fwup_*.deb artifacts/$ARTIFACT_SUBDIR/
        ;;
    Linux-singlethread)
        # This is just for testing
        ;;
esac

# If something goes wrong with the deploy on travis, this helps a lot
ls -las artifacts
ls -las artifacts/$ARTIFACT_SUBDIR
