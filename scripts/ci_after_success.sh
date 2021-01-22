#!/bin/bash

#
# Handle a successful build on Travis
#
# Inputs:
#    MODE           - "dynamic", etc.
#

set -e
set -v

OS_NAME="$(uname -s)"


if [[ "$OS_NAME" = "Linux" ]]; then
    if [[ "$MODE" = "dynamic" ]]; then
        coveralls --exclude tests --exclude 3rdparty --exclude /usr/include --exclude confuse-3.0 --exclude-pattern "fwup-.*-dev" --gcov-options '\-lp'
    fi
fi

