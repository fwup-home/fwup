#!/bin/sh
# This script is used to test the execute command
fname=$1
shift
cat "$@" > $fname
