#!/bin/bash

#
# Deploy a successful build
#
# Inputs:
#  CIRCLECI            - On CircleCI, this is "true"
#  CIRCLE_PULL_REQUEST - Must be unset. CircleCI should not invoke this script on pull requests
#  CIRCLE_TAG          - The tag name if a tagged build
#  CIRCLE_BRANCH       - The branch name
#  CIRCLE_OS_NAME      - "linux" or "osx"
#  MODE                - "static", "dynamic", "singlethread", or "windows"
#

set -e
set -v

if [[ "$CIRCLECI" != "true" ]]; then
    echo "This script is intended to be run on CircleCI"
    exit 1
fi

if [[ -n "$CIRCLE_PULL_REQUEST" ]]; then
    echo "Not supposed to be deploying pull requests!!"
    exit 1
fi

if [[ -n "$CIRCLE_TAG" ]]; then
    ARTIFACT_SUBDIR=.
else
    ARTIFACT_SUBDIR=$CIRCLE_BRANCH
fi

# Copy the artifacts to a location that's easy to reference in the .travis.yml
rm -fr artifacts
mkdir -p artifacts/$ARTIFACT_SUBDIR

case "${CIRCLE_OS_NAME}-${MODE}" in
    linux-static)
        cp fwup-*.rpm artifacts/$ARTIFACT_SUBDIR/
        cp fwup_*.deb artifacts/$ARTIFACT_SUBDIR/
        cp fwup-*.tar.gz artifacts/$ARTIFACT_SUBDIR/
        ;;
    linux-windows)
        cp fwup.exe artifacts/$ARTIFACT_SUBDIR/
        cp fwup.*.nupkg artifacts/$ARTIFACT_SUBDIR/
        ;;
    linux-raspberrypi)
        cp fwup_*.deb artifacts/$ARTIFACT_SUBDIR/
        ;;
    linux-singlethread)
        # This is just for testing
        ;;
esac

# If something goes wrong with the deploy on CircleCI, this helps a lot
ls -las artifacts
ls -las artifacts/$ARTIFACT_SUBDIR
