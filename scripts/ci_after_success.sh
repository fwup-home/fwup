#!/bin/bash

#
# Handle a successful build on CircleCI
#
# Inputs:
#    CIRCLE_OS_NAME - "linux" or "osx"
#    MODE           - "dynamic", etc.
#

set -e
set -v

if [[ "$CIRCLE_OS_NAME" = "linux" ]]; then
    if [[ "$MODE" = "dynamic" ]]; then
        coveralls --exclude tests --exclude 3rdparty --exclude /usr/include --exclude confuse-3.0 --exclude-pattern "fwup-.*-dev" --gcov-options '\-lp'
    fi
fi

