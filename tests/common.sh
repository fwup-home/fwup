#!/bin/bash

# Common unit test commands
set -e

TESTS_DIR=$(dirname $(readlink -f $0))

WORK=$TESTS_DIR/work
FWUP=$TESTS_DIR/../src/fwup
RESULTS=$WORK/results

[ -e $FWUP ] || ( echo "Build $FWUP first"; exit 1 )

CONFIG=$WORK/fwup.conf
FWFILE=$WORK/fwup.fw
IMGFILE=$WORK/fwup.img
UNZIPDIR=$WORK/unzip
EXPECTED_META_CONF=$WORK/meta.conf.expected
TRIMMED_META_CONF=$WORK/meta.conf.trimmed

# Setup the directories
rm -fr $WORK
mkdir -p $WORK
mkdir -p $UNZIPDIR

unzip_fw() {
    unzip -q $FWFILE -d $UNZIPDIR
}

check_meta_conf() {
    # Need to unzip the fw file to check the meta.conf
    if [ -e $UNZIPDIR/meta.conf ]; then
        : good
    else
        unzip_fw
    fi

    # Trim the results of known lines that vary between runs
    cat $UNZIPDIR/meta.conf | \
        grep -v "^#" | \
        grep -v "^meta-creation-date=" | \
        grep -v "host-path=" \
        > $TRIMMED_META_CONF
    diff $EXPECTED_META_CONF $TRIMMED_META_CONF
}
