#!/bin/bash

#
# Deploy a successful build on Travis
#
# Inputs:
#  TRAVIS              - On Travis-CI, this is "true"
#  TRAVIS_PULL_REQUEST - Must be "false". Travis should not invoke this script on pull requests
#  TRAVIS_TAG          - The tag name if a tagged build
#  TRAVIS_BRANCH       - The branch name
#  TRAVIS_OS_NAME - "linux" or "osx"
#  BUILD_STATIC   - "true" or "false"
#

set -e
set -v

if [[ "$TRAVIS" != "true" ]]; then
    echo "This script is intended to be run on Travis"
    exit 1
fi

if [[ "$TRAVIS_PULL_REQUEST" != "false" ]]; then
    echo "Not supposed to be deploying pull requests!!"
    exit 1
fi

if [[ -n "$TRAVIS_TAG" ]]; then
    ARTIFACT_SUBDIR=.
else
    ARTIFACT_SUBDIR=$TRAVIS_BRANCH
fi

# Copy the artifacts to a location that's easy to reference in the .travis.yml
rm -fr artifacts
mkdir -p artifacts/$ARTIFACT_SUBDIR

if [[ "$TRAVIS_OS_NAME" = "linux" && "$BUILD_STATIC" = "true" ]]; then
    cp fwup-*.rpm artifacts/$ARTIFACT_SUBDIR/
    cp fwup_*.deb artifacts/$ARTIFACT_SUBDIR/
    cp fwup-*.msi artifacts/$ARTIFACT_SUBDIR/
    cp fwup.exe artifacts/$ARTIFACT_SUBDIR/
    cp fwup-*.tar.gz artifacts/$ARTIFACT_SUBDIR/
fi

# If something goes wrong with the deploy on travis, this helps a lot
ls -las artifacts
ls -las artifacts/*
