#!/bin/bash

#
# Handle a successful build on Travis
#
# Inputs:
#    TRAVIS_OS_NAME - "linux" or "osx"
#    MODE           - "dynamic", etc.
#

set -e
set -v

if [[ "$TRAVIS_OS_NAME" = "linux" ]]; then
    if [[ "$MODE" = "dynamic" ]]; then
        coveralls --exclude tests --exclude 3rdparty --exclude /usr/include --exclude confuse-3.0 --exclude libsodium --exclude-pattern "fwup-.*-dev" --exclude-pattern "libsodium-1.0" --gcov-options '\-lp'
    fi
fi

