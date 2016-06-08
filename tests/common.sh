#!/bin/bash

# Common unit test commands
set -e

export LC_ALL=C

# Linux command line tools that may be different on other OSes
READLINK=readlink
SED=sed

case "$OSTYPE" in
    darwin*)
        [ -e /usr/local/bin/greadlink ] || ( echo "Please run 'brew install coreutils'"; exit 1 )
        [ -e /usr/local/bin/mdir ] || ( echo "Please run 'brew install mtools'"; exit 1 )
        [ -e /usr/local/bin/gsed ] || ( echo "Please run 'brew install gnu-sed'"; exit 1 )

        READLINK=/usr/local/bin/greadlink
        SED=/usr/local/bin/gsed
        ;;
    *)
        ;;
esac

TESTS_DIR=$(dirname $($READLINK -f $0))

# Default to testing the fwup built in the src directory,
# but it is possible to define the version of fwup used for
# the create and apply steps separately.
FWUP_DEFAULT=$TESTS_DIR/../src/fwup
if [ -z $FWUP_CREATE ]; then FWUP_CREATE=$FWUP_DEFAULT; fi
if [ -z $FWUP_APPLY ]; then FWUP_APPLY=$FWUP_DEFAULT; fi

WORK=$TESTS_DIR/work
RESULTS=$WORK/results

[ -e $FWUP_CREATE ] || ( echo "Can't find $FWUP_CREATE"; exit 1 )
[ -e $FWUP_APPLY ] || ( echo "Can't find $FWUP_APPLY"; exit 1 )

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
        grep -v "^meta-creation-date" | \
        grep -v "^meta-fwup-version" \
        > $TRIMMED_META_CONF
    diff -w $EXPECTED_META_CONF $TRIMMED_META_CONF
}

# Test input files

# These files contain random data so that it is possible to
# verify that fwup copied things correctly. Previously fwup
# unit tests used files filled with 1s, and it possible for
# a test to pass even though the data had been reordered.
# (This never actually happened to my knowledge.)

TESTFILE_1K=$TESTS_DIR/1K.bin
TESTFILE_1K_CORRUPT=$TESTS_DIR/1K-corrupt.bin
TESTFILE_150K=$TESTS_DIR/150K.bin

# Generated test data
TESTFILE_15M=$TESTS_DIR/15M.bin
if [ ! -e $TESTFILE_15M ]; then
    for i in {1..100}; do
        cat $TESTFILE_150K >> $TESTFILE_15M
    done
fi
