#!/bin/sh

set -e

mkdir -p m4 # silence autoreconf warning
autoreconf --install
