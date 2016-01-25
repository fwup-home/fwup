#!/bin/sh

set -e

mkdir m4 # silence autoreconf warning
autoreconf --install
