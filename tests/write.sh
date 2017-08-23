#!/bin/bash
fname=$1
shift
cat "$@" > $fname
